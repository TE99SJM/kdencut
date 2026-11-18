// SPDX-License-Identifier: GPL-3.0-or-later
// Lens projection adapted from PanoForge, copyright 2026 Emmanuel Wenner (MIT).
#include "osmo.h"
#include <QSaveFile>
#include <QtEndian>
#include <QtMath>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace Osmo {
using Fields = QMap<int, QList<QByteArray>>;
static Fields fields(const QByteArray &data)
{
    qsizetype pos = 0;
    auto integer = [&] {
        quint64 v = 0;
        for (int shift = 0; shift <= 63 && pos < data.size(); shift += 7) {
            quint8 b = data[pos++];
            if (shift == 63 && b > 1) break;
            v |= quint64(b & 127) << shift;
            if (!(b & 128)) return v;
        }
        fail("Invalid DJI metadata integer.");
        return quint64(0);
    };
    Fields out;
    while (pos < data.size()) {
        quint64 tag = integer(), wire = tag & 7;
        if (!tag || tag >> 3 > 536870911) fail("Invalid DJI field.");
        if (wire == 0) {
            integer();
            continue;
        }
        if (wire != 1 && wire != 2 && wire != 5) fail("Unknown DJI metadata field.");
        quint64 n = wire == 2 ? integer() : wire == 1 ? 8 : 4;
        if (n > quint64(data.size() - pos)) fail("Incomplete DJI metadata.");
        out[int(tag >> 3)].append(data.mid(pos, n));
        pos += n;
    }
    return out;
}
std::vector<Lens> readCalibration(const QByteArray &packet, int width)
{
    auto inner = fields(fields(packet).value(2).value(0));
    auto groups = fields(inner.value(6).value(0));
    std::vector<Lens> result;
    for (const auto &entries : groups)
        for (const auto &entry : entries) {
            auto lens = fields(entry);
            if (!lens.contains(21)) continue;
            auto numbers = [&](int key) {
                auto bytes = lens.value(key).value(0);
                if (bytes.size() % 4) fail("Invalid lens calibration.");
                QList<double> values;
                for (int i = 0; i < bytes.size(); i += 4) {
                    quint32 bits = qFromLittleEndian<quint32>(bytes.constData() + i);
                    float f;
                    std::memcpy(&f, &bits, 4);
                    if (!std::isfinite(f)) fail("Invalid lens calibration.");
                    values << f;
                }
                return values;
            };
            auto first = [&](int key) {
                auto list = numbers(key);
                if (list.isEmpty()) fail("Missing lens calibration.");
                return list.first();
            };
            double sensor = first(10);
            if (sensor <= 0) fail("Invalid calibration dimensions.");
            double scale = width / sensor;
            Lens l{};
            l.fx = first(1) * scale;
            l.cx = first(3) * scale;
            l.cy = first(4) * scale;
            l.height = first(11) * scale;
            if (l.fx <= 0 || l.height <= 0) fail("Invalid lens dimensions.");
            for (int i = 0; i < 4; ++i)
                l.dist[i] = first(5 + i);
            auto q = numbers(21);
            if (q.size() != 4) fail("Missing lens orientation.");
            l.rotation = QQuaternion(q[0], q[1], q[2], q[3]);
            if (l.rotation.lengthSquared() <= .8 || l.rotation.lengthSquared() >= 1.2) fail("Invalid lens orientation.");
            l.rotation.normalize();
            auto x = numbers(22), y = numbers(23);
            if (x.size() == y.size() && x.size() >= 14) {
                for (int i = 1; i < x.size(); ++i)
                    l.seamRadius += std::hypot(x[i] * scale - l.cx, y[i] * scale - l.cy);
                l.seamRadius /= x.size() - 1;
            }
            result.push_back(l);
            if (result.size() == 2) return result;
        }
    return {};
}
struct Projection
{
    Lens lens;
    QMatrix3x3 matrix;
    double scale, slope, boundary;
    explicit Projection(const Lens &l)
        : lens(l)
        , matrix(l.rotation.toRotationMatrix())
    {
        const double t = qDegreesToRadians(85.);
        const auto &k = l.dist;
        slope = 1 + 3 * k[0] * std::pow(t, 2) + 5 * k[1] * std::pow(t, 4) + 7 * k[2] * std::pow(t, 6) + 9 * k[3] * std::pow(t, 8);
        boundary = polynomial(t);
        double r90 = l.fx * radial(qDegreesToRadians(90.));
        scale = l.seamRadius > 0 && r90 > 0 ? l.seamRadius / r90 : 1;
    }
    double polynomial(double t) const
    {
        const auto &k = lens.dist;
        double u = t * t;
        return t * (1 + u * (k[0] + u * (k[1] + u * (k[2] + u * k[3]))));
    }
    double radial(double t) const
    {
        const double limit = qDegreesToRadians(85.);
        return t <= limit ? polynomial(t) : boundary + slope * (t - limit);
    }
};
void maps(const Clip &clip, int w, int lensWidth, const QString &folder, const Stop &stop)
{
    QDir dir(folder);
    if (dir.exists("blend_mask.pgm")) return;
    if (!dir.mkpath(".")) fail("Could not create temporary lens map directory.");
    int h = w / 2;
    QStringList names{"xmap0", "ymap0", "xmap1", "ymap1", "blend_mask"};
    std::array<std::unique_ptr<QSaveFile>, 5> out;
    std::array<QByteArray, 5> rows;
    for (int i = 0; i < 5; ++i) {
        out[i] = std::make_unique<QSaveFile>(dir.filePath(names[i] + ".pgm"));
        QByteArray header = QString("P5\n%1 %2\n%3\n").arg(w).arg(h).arg(i == 4 ? 255 : 65535).toLatin1();
        if (!out[i]->open(QIODevice::WriteOnly) || out[i]->write(header) != header.size()) fail(out[i]->errorString());
        rows[i].resize(w * (i == 4 ? 1 : 2));
    }
    std::array<Projection, 2> projections{Projection(clip.calibration.at(0)), Projection(clip.calibration.at(1))};
    double factor = double(lensWidth) / clip.width;
    std::vector<double> sx(w), cx(w);
    for (int x = 0; x < w; ++x) {
        double lon = ((x + .5) / w * 2 - .5) * M_PI;
        sx[x] = std::sin(lon);
        cx[x] = std::cos(lon);
    }
    for (int y = 0; y < h; ++y) {
        check(stop);
        double lat = (.5 - (y + .5) / h) * M_PI, c = std::cos(lat), z = std::sin(lat);
        for (int x = 0; x < w; ++x) {
            double weights[2]{}, dx = c * sx[x], dy = c * cx[x];
            for (int i = 0; i < 2; ++i) {
                const auto &p = projections[i];
                const auto &m = p.matrix;
                double lx = m(0, 0) * dx + m(0, 1) * dy + m(0, 2) * z, ly = m(1, 0) * dx + m(1, 1) * dy + m(1, 2) * z;
                double theta = std::acos(std::clamp(m(2, 0) * dx + m(2, 1) * dy + m(2, 2) * z, -1., 1.));
                double radius = p.scale * p.lens.fx * p.radial(theta) / std::max(1e-12, std::hypot(lx, ly));
                double px = (p.lens.cx + radius * lx) * factor, py = (p.lens.cy + radius * ly) * factor;
                bool valid = theta < qDegreesToRadians(96.) && px >= 0 && py >= 0 && px <= lensWidth - 1 && py <= p.lens.height * factor - 1;
                qToBigEndian<quint16>(valid ? std::nearbyint(px) : 65535, rows[i * 2].data() + x * 2);
                qToBigEndian<quint16>(valid ? std::nearbyint(py) : 65535, rows[i * 2 + 1].data() + x * 2);
                if (valid) weights[i] = std::clamp((95. - qRadiansToDegrees(theta)) / 10., 0., 1.);
            }
            double sum = weights[0] + weights[1];
            rows[4][x] = char(std::nearbyint(255 * (sum > 0 ? weights[1] / sum : .5)));
        }
        for (int i = 0; i < 5; ++i)
            if (out[i]->write(rows[i]) != rows[i].size()) fail(out[i]->errorString());
    }
    for (auto &file : out)
        if (!file->commit()) fail(file->errorString());
}
} // namespace Osmo
