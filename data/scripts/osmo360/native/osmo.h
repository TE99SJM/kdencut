// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QDir>
#include <QImage>
#include <QJsonObject>
#include <QMap>
#include <QQuaternion>
#include <QVector3D>
#include <atomic>
#include <functional>
#include <memory>
#include <stdexcept>
#include <vector>

namespace Osmo {
using Stop = std::shared_ptr<std::atomic_bool>;
using Progress = std::function<void(int)>;
inline void fail(const QString &message)
{
    throw std::runtime_error(message.toUtf8().constData());
}
inline void check(const Stop &stop)
{
    if (stop && stop->load()) fail("Cancelled.");
}
struct Lens
{
    double fx, cx, cy, height;
    double dist[4];
    QQuaternion rotation;
    double seamRadius;
};
struct Clip
{
    QString path, rate;
    int lens[2]{}, width = 0, frames = 0;
    double fps = 0;
    std::vector<Lens> calibration;
    QStringList color;
};
struct View
{
    double yaw = 0, pitch = 0, roll = 0, fov = 90;
};
struct Roll
{
    double angle;
    bool smooth;
};
struct Track
{
    QMap<int, View> aim{{0, {}}};
    QMap<int, Roll> rolls;
    bool panOnly = true;
    View at(int frame) const;
    QList<int> frames() const;
};
struct Edit
{
    Clip clip;
    Track track;
    QString mode = "Landscape";
};
inline const QStringList modes{"Landscape", "Portrait", "Square", "360 panorama"};
QSize outputSize(const QString &mode, bool high, int lensWidth);
Edit openEdit(const QString &path);
void saveEdit(const QString &path, const Edit &edit);
QByteArray run(const QString &program, const QStringList &args, const QString &dir = {}, const Stop &stop = {}, const Progress &progress = {});
Clip probe(const QString &path);
std::vector<Lens> readCalibration(const QByteArray &packet, int width);
void maps(const Clip &clip, int width, int lensWidth, const QString &folder, const Stop &stop);
struct Proxy
{
    QString path;
    int frames = 0;
};
Proxy makeProxy(const Clip &clip, const QString &dir, const Stop &stop, const Progress &progress);
QImage preview(const Clip &clip, int frame, View view, QSize size, bool sphere, const Proxy &proxy, const QString &dir, const Stop &stop);
void render(const Edit &edit, QSize size, const QString &output, const Stop &stop, const Progress &progress);
void tagSpherical(const QString &path);
void installJob(const QString &root = {});
} // namespace Osmo
