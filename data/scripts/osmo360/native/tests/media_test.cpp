// SPDX-License-Identifier: GPL-3.0-or-later
#include "../osmo.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <cstdio>
using namespace Osmo;
static void expect(bool ok, const char *message)
{
    if (!ok) fail(message);
}
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    try {
        QTemporaryDir temp;
        auto file = [&](const QString &name) { return temp.filePath(name); };
        QString source = file("camera with spaces.OSV");
        run("ffmpeg", {"-v",       "error",
                       "-f",       "lavfi",
                       "-i",       "testsrc2=s=64x64:r=30000/1001",
                       "-f",       "lavfi",
                       "-i",       "smptebars=s=64x64:r=30000/1001",
                       "-f",       "lavfi",
                       "-i",       "sine=frequency=440",
                       "-map",     "0:v",
                       "-map",     "1:v",
                       "-map",     "2:a",
                       "-t",       "0.34",
                       "-c:v",     "libx264",
                       "-threads", "1",
                       "-c:a",     "aac",
                       "-f",       "mp4",
                       source});
        auto e = openEdit(source);
        auto &c = e.clip;
        expect(c.frames == 10 && c.rate == "30000/1001", "Incorrect source timing");
        e.track.aim[9] = {120, 0, 0, 80};
        e.track.rolls[5] = {-27, false};
        e.track.rolls[8] = {18, false};
        expect(e.track.at(4).roll == 0 && e.track.at(5).roll == -27 && e.track.at(7).roll == -27 && e.track.at(8).roll == 18, "Roll was not held");
        saveEdit(file("camera.k360"), e);
        // An already cancelled open must not start FFprobe for either input format.
        const auto cancelled = std::make_shared<std::atomic_bool>(true);
        for (const auto &path : {source, file("camera.k360")}) {
            bool stopped = false;
            try {
                openEdit(path, cancelled);
            } catch (const std::runtime_error &error) {
                stopped = QString::fromUtf8(error.what()) == "Cancelled.";
            }
            expect(stopped, "Clip inspection ignored cancellation");
        }
        auto again = openEdit(file("camera.k360"));
        expect(again.track.panOnly && again.track.at(7).roll == -27, "Track round trip failed");
        auto proxy = makeProxy(c, temp.path(), {}, {});
        expect(proxy.frames >= 1, "Missing playback proxy");
        QSize size(160, 90);
        for (int n : {0, 5, c.frames - 1}) {
            auto v = e.track.at(n);
            auto original = preview(c, n, v, size, false, {}, temp.path(), {});
            expect(original.size() == size && preview(c, n, v, size, false, proxy, temp.path(), {}).size() == size, "Missing preview");
            Proxy broken{file("missing.mp4"), 9};
            expect(preview(c, n, v, size, false, broken, temp.path(), {}) == original, "Unreadable proxy did not fall back to the source");
        }
        for (bool sphere : {false, true}) {
            e.mode = sphere ? "360 panorama" : "Landscape";
            QString out = file(sphere ? "sphere.mp4" : "flat.mp4");
            auto dimensions = sphere ? QSize(128, 64) : outputSize(e.mode, true, c.width);
            render(e, dimensions, out, {}, {});
            auto data = run("ffprobe", {"-v", "error", "-show_streams", "-of", "json", out});
            auto streams = QJsonDocument::fromJson(data).object()["streams"].toArray();
            auto v = streams.first().toObject();
            expect(v["width"].toInt() == dimensions.width() && v["height"].toInt() == dimensions.height() && v["nb_frames"].toString().toInt() == c.frames &&
                       streams.last().toObject()["codec_name"] == "aac",
                   "Export dimensions, audio or frames changed");
            expect(data.contains("equirectangular") == sphere, "Incorrect spherical metadata");
        }
        return 0;
    } catch (const std::exception &ex) {
        std::fprintf(stderr, "%s\n", ex.what());
        return 1;
    }
}
