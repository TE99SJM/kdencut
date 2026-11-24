// SPDX-License-Identifier: GPL-3.0-or-later
#include "osmo.h"
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtMath>
#include <cmath>
#include <numeric>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cstdio>
#endif

namespace Osmo {
static QString toolPath(const QString &program)
{
    auto paths = QStringList{QCoreApplication::applicationDirPath()};
#ifdef Q_OS_WIN
    paths << QDir(qEnvironmentVariable("ProgramFiles")).filePath("kdenlive/bin");
#endif
    QString bin = QStandardPaths::findExecutable(program, paths);
    if (bin.isEmpty()) bin = QStandardPaths::findExecutable(program);
    if (bin.isEmpty()) fail("Could not find " + program + ". Install FFmpeg or add its bin folder to PATH.");
    return bin;
}
QByteArray run(const QString &program, const QStringList &args, const QString &dir, const Stop &stop, const Progress &progress)
{
    check(stop);
    QProcess p;
    p.setWorkingDirectory(dir);
    p.setStandardInputFile(QProcess::nullDevice());
    p.start(toolPath(program), args);
    if (!p.waitForStarted()) fail(p.errorString());
    QByteArray data, errors;
    auto drain = [&] {
        errors += p.readAllStandardError();
        errors = errors.right(4000);
        data += p.readAllStandardOutput();
        if (progress) {
            qsizetype i;
            while ((i = data.indexOf('\n')) >= 0) {
                auto line = data.left(i).trimmed();
                data.remove(0, i + 1);
                if (line.startsWith("frame=")) progress(line.mid(6).toInt());
            }
        }
    };
    while (p.state() != QProcess::NotRunning) {
        if (stop && stop->load()) {
            p.kill();
            p.waitForFinished();
            check(stop);
        }
        p.waitForFinished(80);
        drain();
    }
    drain();
    check(stop);
    if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0)
        fail(errors.trimmed().isEmpty() ? program + " exited without completing." : QString::fromUtf8(errors.trimmed()));
    return data;
}
static QJsonObject inspect(const QString &path, QStringList flags, const Stop &stop = {})
{
    auto raw = run("ffprobe", QStringList{"-v", "error"} + flags + QStringList{"-of", "json", path}, {}, stop);
    QJsonParseError e;
    auto doc = QJsonDocument::fromJson(raw, &e);
    if (e.error || !doc.isObject()) fail("Invalid FFprobe response.");
    return doc.object();
}
Clip probe(const QString &path, const Stop &stop)
{
    Clip c;
    c.path = QFileInfo(path).absoluteFilePath();
    auto json = inspect(c.path, {"-show_streams", "-show_format"}, stop);
    QList<QJsonObject> video;
    QList<int> metadata;
    for (auto stream : json["streams"].toArray()) {
        auto s = stream.toObject();
        if (s["codec_tag_string"] == "djmd") metadata << s["index"].toInt();
        if (s["codec_type"] == "video" && s["width"].toInt() > 0 && s["width"] == s["height"] && !s["disposition"].toObject()["attached_pic"].toInt())
            video << s;
    }
    if (video.size() != 2 || video[0]["width"] != video[1]["width"] || video[0]["r_frame_rate"] != video[1]["r_frame_rate"])
        fail("Choose an original Osmo recording with two matching square video tracks.");
    auto first = video.first();
    auto rate = first["r_frame_rate"].toString().split('/');
    int n = rate.value(0).toInt(), d = rate.value(1, "1").toInt();
    if (d <= 0 || n <= 0 || double(n) / d > 240) fail("Unsupported camera frame rate.");
    int divisor = std::gcd(n, d);
    n /= divisor;
    d /= divisor;
    c.rate = QString::number(n);
    if (d != 1) c.rate += "/" + QString::number(d);
    c.fps = double(n) / d;
    c.frames = first["nb_frames"].toString().toInt();
    if (c.frames <= 0) c.frames = qRound(first["duration"].toString(json["format"].toObject()["duration"].toString()).toDouble() * c.fps);
    if (c.frames <= 0) fail("The recording has no readable video frames.");
    c.width = first["width"].toInt();
    for (int i = 0; i < 2; ++i)
        c.lens[i] = video[i]["index"].toInt();
    for (const auto &entry :
         QList<QPair<QString, QString>>{{"color_space", "-colorspace"}, {"color_primaries", "-color_primaries"}, {"color_transfer", "-color_trc"}}) {
        auto value = first[entry.first].toString();
        if (!value.isEmpty() && !QStringList{"unknown", "reserved", "unspecified"}.contains(value)) c.color << entry.second << value;
    }
    if (!metadata.isEmpty()) {
        auto packets =
            inspect(c.path, {"-select_streams", "d", "-read_intervals", "%+1", "-show_packets", "-show_entries", "packet=stream_index,pos,size"}, stop)["packets"]
                .toArray();
        QFile file(c.path);
        if (!file.open(QIODevice::ReadOnly)) fail(file.errorString());
        for (auto packet : packets) {
            check(stop);
            auto p = packet.toObject();
            if (!metadata.contains(p["stream_index"].toInt()) || !p.contains("pos")) continue;
            int bytes = p["size"].toString().toInt();
            if (bytes <= 0 || bytes > 4000000 || !file.seek(p["pos"].toString().toLongLong())) continue;
            try {
                c.calibration = readCalibration(file.read(bytes), c.width);
            } catch (const std::runtime_error &) {
                continue;
            }
            if (c.calibration.size() == 2) break;
        }
    }
    return c;
}
static QString number(double n)
{
    return QString::number(n, 'f', 9);
}
static QStringList base()
{
    return {"-hide_banner", "-v", "error", "-nostdin", "-y", "-filter_complex_threads", "3", "-filter_threads", "2"};
}
static QStringList seek(int frame, double fps)
{
    // Seek inside the interval preceding the desired PTS. Decimal conversion
    // must not round past the last available frame and return an empty image.
    return frame > 0 ? QStringList{"-ss", number((frame - .25) / fps)} : QStringList{};
}
struct Graph
{
    QStringList args;
    QString filter;
};
static Graph stitch(const Clip &c, int w, const QString &dir, int frame, const Stop &stop, bool proxy = false)
{
    Graph g{base() + QStringList{"-threads", "1", "-thread_type", "slice"} + seek(frame, c.fps) + QStringList{"-i", c.path}, {}};
    auto a = QString("[0:%1]").arg(c.lens[0]), b = QString("[0:%1]").arg(c.lens[1]);
    QString sample = proxy ? "fps=8:eof_action=pass," : "";
    if (c.calibration.empty()) {
        g.filter = a + b + "hstack," + sample + QString("v360=dfisheye:e:ih_fov=190:iv_fov=190:yaw=90:w=%1:h=%2:interp=line,setsar=1").arg(w).arg(w / 2);
    } else {
        int size = std::min(c.width, std::max(512, w / 2));
        QString folder = QDir(dir).filePath("maps-" + QString::number(w));
        maps(c, w, size, folder, stop);
        for (const auto &name : QStringList{"xmap0", "ymap0", "xmap1", "ymap1", "blend_mask"})
            g.args << "-i" << QDir(folder).filePath(name + ".pgm");
        QString prep = sample + QString("scale=%1:%1:flags=bicubic,format=gbrp16le").arg(size);
        g.filter = a + prep + "[a];[a][1:v][2:v]remap[l];" + b + prep + "[b];[b][3:v][4:v]remap[r];[5:v]format=gbrp16le[m];[l][r][m]maskedmerge,setsar=1";
    }
    return g;
}
static QStringList angles(View v, QSize size)
{
    double vertical = qRadiansToDegrees(2 * std::atan(std::tan(qDegreesToRadians(v.fov / 2)) * size.height() / size.width()));
    return {number(v.yaw), number(v.pitch), number(v.roll), number(v.fov), number(vertical)};
}
static const QStringList names{"yaw", "pitch", "roll", "h_fov", "v_fov"};
static QString viewport(View v, QSize size)
{
    QString result = QString("v360@view=e:flat:w=%1:h=%2:interp=line:reset_rot=1").arg(size.width()).arg(size.height());
    auto values = angles(v, size);
    for (int i = 0; i < names.size(); ++i)
        result += ":" + names[i] + "=" + values[i];
    return result;
}
QImage preview(const Clip &c, int frame, View view, QSize size, bool sphere, const Proxy &proxy, const QString &dir, const Stop &stop)
{
    auto read = [&](Graph g) {
        g.filter += "," + (sphere ? QString("scale=%1:%2").arg(size.width()).arg(size.height()) : viewport(view, size)) + "[view]";
        auto bytes = run("ffmpeg",
                         g.args + QStringList{"-filter_complex", g.filter, "-map", "[view]", "-frames:v", "1", "-pix_fmt", "rgb24", "-c:v", "rawvideo",
                                              "-threads", "1", "-f", "rawvideo", "pipe:1"},
                         dir, stop);
        if (bytes.size() != size.width() * size.height() * 3) fail(QString("No complete preview image at frame %1.").arg(frame));
        return QImage(reinterpret_cast<const uchar *>(bytes.constData()), size.width(), size.height(), size.width() * 3, QImage::Format_RGB888).copy();
    };
    if (proxy.frames > 0) {
        int index = std::min(proxy.frames - 1, int(std::floor(frame * 8 / c.fps)));
        try {
            return read({base() + seek(index, 8) + QStringList{"-i", proxy.path}, "[0:v:0]null"});
        } catch (const std::runtime_error &) {
            check(stop);
        }
    }
    return read(stitch(c, 1024, dir, frame, stop));
}
Proxy makeProxy(const Clip &c, const QString &dir, const Stop &stop, const Progress &progress)
{
    auto g = stitch(c, 1024, dir, 0, stop, true);
    QString path = QDir(dir).filePath("preview.mp4");
    run("ffmpeg",
        g.args + QStringList{"-filter_complex", g.filter + "[v]", "-map", "[v]", "-an", "-c:v", "libx264", "-g", "1", "-pix_fmt", "yuv420p", "-preset",
                             "ultrafast", "-threads", "3", "-progress", "pipe:1", "-nostats", path},
        dir, stop, progress);
    auto streams = inspect(path, {"-select_streams", "v:0", "-show_entries", "stream=nb_frames"}, stop)["streams"].toArray();
    int frames = streams.isEmpty() ? 0 : streams.first().toObject()["nb_frames"].toString().toInt();
    if (frames < 1) fail("Playback proxy contains no video frames.");
    return {path, frames};
}
void render(const Edit &edit, QSize size, const QString &output, const Stop &stop, const Progress &progress)
{
    const auto &c = edit.clip;
    QFileInfo target(output);
    if (target.suffix().compare("mp4", Qt::CaseInsensitive) || target == QFileInfo(c.path)) fail("Choose an MP4 destination.");
    QTemporaryDir temp(target.dir().filePath(".osmo360-XXXXXX"));
    if (!temp.isValid()) fail("Cannot write to the export directory.");
    bool sphere = edit.mode == "360 panorama";
    auto g = stitch(c, sphere ? size.width() : std::min(2 * c.width, std::max(1024, 2 * std::max(size.width(), size.height()))), temp.path(), 0, stop);
    if (!sphere) {
        QFile cmds(temp.filePath("camera.cmd"));
        if (!cmds.open(QIODevice::WriteOnly)) fail(cmds.errorString());
        auto prev = angles(edit.track.at(0), size);
        int last = edit.track.frames().last();
        for (int i = 1; i <= last; ++i) {
            check(stop);
            auto current = angles(edit.track.at(i), size);
            QStringList changed;
            for (int j = 0; j < names.size(); ++j)
                if (current[j] != prev[j]) changed << "v360@view " + names[j] + " " + current[j];
            if (!changed.isEmpty()) {
                auto line = (number((i - .5) / c.fps) + " " + changed.join(", ") + ";\n").toLatin1();
                if (cmds.write(line) != line.size()) fail(cmds.errorString());
            }
            prev = current;
        }
        bool moving = cmds.size() > 0;
        cmds.close();
        g.filter += ",setpts=PTS-STARTPTS,";
        if (moving) g.filter += "sendcmd=f=camera.cmd,";
        g.filter += viewport(edit.track.at(0), size);
    }
    QString stage = temp.filePath("render.mp4");
    g.args << "-filter_complex" << g.filter + "[v]" << "-map" << "[v]" << "-map" << "0:a?" << "-map_metadata" << "-1" << "-r" << c.rate << "-fps_mode" << "cfr"
           << "-t" << number(c.frames / c.fps) << "-c:a" << "copy";
    if (sphere)
        g.args << "-c:v" << "libx265" << "-pix_fmt" << "yuv420p10le" << "-tag:v" << "hvc1" << "-preset" << "fast" << "-x265-params"
               << "log-level=error:pools=none:frame-threads=1:rc-lookahead=0:bframes=0";
    else
        g.args << "-c:v" << "libx264" << "-pix_fmt" << "yuv420p" << "-preset" << "veryfast";
    g.args << "-crf" << "18" << c.color << "-threads" << "4" << "-progress" << "pipe:1" << "-nostats" << stage;
    run("ffmpeg", g.args, temp.path(), stop, progress);
    if (sphere) tagSpherical(stage);
    check(stop);
#ifdef Q_OS_WIN
    if (!MoveFileExW(reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(stage).utf16()),
                     reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(target.absoluteFilePath()).utf16()),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        fail(QString("Could not save the completed MP4 (Windows error %1).").arg(GetLastError()));
#else
    if (std::rename(QFile::encodeName(stage).constData(), QFile::encodeName(target.absoluteFilePath()).constData())) fail("Could not save the completed MP4.");
#endif
}
} // namespace Osmo
