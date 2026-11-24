// SPDX-License-Identifier: GPL-3.0-or-later
#include "osmo.h"
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <algorithm>
#include <cmath>

namespace Osmo {
static double wrap(double angle)
{
    return angle - 360 * std::floor((angle + 180) / 360);
}
View Track::at(int frame) const
{
    auto next = aim.upperBound(frame);
    auto prev = next == aim.begin() ? next : std::prev(next);
    View v = prev.value();
    if (next != aim.end() && next != prev) {
        double t = double(frame - prev.key()) / (next.key() - prev.key());
        v.yaw = wrap(v.yaw + t * wrap(next->yaw - v.yaw));
        v.pitch += t * (next->pitch - v.pitch);
        v.fov += t * (next->fov - v.fov);
    }
    auto end = rolls.upperBound(frame);
    auto start = end == rolls.begin() ? rolls.end() : std::prev(end);
    v.roll = start == rolls.end() ? 0 : start->angle;
    if (end != rolls.end() && end->smooth) {
        if (start == rolls.end())
            v.roll = end->angle;
        else if (frame != start.key())
            v.roll = wrap(v.roll + double(frame - start.key()) / (end.key() - start.key()) * wrap(end->angle - v.roll));
    }
    return v;
}
QList<int> Track::frames() const
{
    auto result = aim.keys() + rolls.keys();
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}
QSize outputSize(const QString &mode, bool high, int lensWidth)
{
    int w = high ? 3840 : 1920, h = high ? 2160 : 1080;
    if (mode == "Portrait") return {h, w};
    if (mode == "Square") return {h, h};
    if (mode == "360 panorama") {
        w = std::min(2 * lensWidth, high ? 7680 : 3840);
        return {w, w / 2};
    }
    return {w, h};
}
static double angle(QJsonValue value, double low, double high)
{
    if (!value.isDouble() || value.toDouble() < low || value.toDouble() > high) fail("Invalid camera angle in saved edit.");
    return value.toDouble();
}
Edit openEdit(const QString &path, const Stop &stop)
{
    QFileInfo file(path);
    Edit edit;
    if (file.suffix().compare("k360", Qt::CaseInsensitive)) {
        edit.clip = probe(path, stop);
        return edit;
    }
    QFile input(path);
    if (!input.open(QIODevice::ReadOnly)) fail(input.errorString());
    auto doc = QJsonDocument::fromJson(input.readAll()).object();
    int version = doc["version"].toInt();
    if ((version != 1 && version != 2) || doc["source"].toString().isEmpty()) fail("Unsupported camera track.");
    edit.clip = probe(file.dir().absoluteFilePath(doc["source"].toString()), stop);
    if (doc["fps"].toString() != edit.clip.rate || doc["frames"].toInt() != edit.clip.frames) fail("The recording no longer matches this edit.");
    edit.mode = doc["mode"].toString();
    if (!modes.contains(edit.mode)) fail("Unsupported output shape.");
    auto frame = [&](const QString &key) {
        bool ok;
        int n = key.toInt(&ok);
        if (!ok || n < 0 || n >= edit.clip.frames) fail("Camera keyframe outside recording.");
        return n;
    };
    auto keys = doc["keys"].toObject();
    if (keys.isEmpty()) fail("This camera track contains no aim keys.");
    edit.track.aim.clear();
    for (auto it = keys.begin(); it != keys.end(); ++it) {
        auto a = it.value().toArray();
        if (a.size() != 4) fail("Invalid camera key.");
        int n = frame(it.key());
        edit.track.aim[n] = {angle(a[0], -180, 180), angle(a[1], -90, 90), 0, angle(a[3], 25, 145)};
        if (version == 1) edit.track.rolls[n] = {angle(a[2], -180, 180), true};
    }
    edit.track.panOnly = version == 2 && doc["pan_only"].toBool();
    if (version == 2) {
        if (!doc["pan_only"].isBool() || !doc["rolls"].isObject()) fail("Invalid camera track.");
        auto rolls = doc["rolls"].toObject();
        for (auto it = rolls.begin(); it != rolls.end(); ++it) {
            auto a = it.value().toArray();
            if (a.size() != 2 || !a[1].isBool()) fail("Invalid roll key.");
            edit.track.rolls[frame(it.key())] = {angle(a[0], -180, 180), a[1].toBool()};
        }
    }
    return edit;
}
void saveEdit(const QString &path, const Edit &edit)
{
    QJsonObject keys, rolls;
    for (auto i = edit.track.aim.cbegin(); i != edit.track.aim.cend(); ++i)
        keys[QString::number(i.key())] = QJsonArray{i->yaw, i->pitch, 0, i->fov};
    for (auto i = edit.track.rolls.cbegin(); i != edit.track.rolls.cend(); ++i)
        rolls[QString::number(i.key())] = QJsonArray{i->angle, i->smooth};
    QJsonObject doc{{"version", 2},          {"source", QFileInfo(path).dir().relativeFilePath(edit.clip.path)},
                    {"fps", edit.clip.rate}, {"frames", edit.clip.frames},
                    {"mode", edit.mode},     {"pan_only", edit.track.panOnly},
                    {"keys", keys},          {"rolls", rolls}};
    QSaveFile out(path);
    auto bytes = QJsonDocument(doc).toJson();
    if (!out.open(QIODevice::WriteOnly) || out.write(bytes) != bytes.size() || !out.commit()) fail(out.errorString());
}
} // namespace Osmo
