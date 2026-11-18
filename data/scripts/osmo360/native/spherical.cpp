// SPDX-License-Identifier: GPL-3.0-or-later
#include "osmo.h"
#include <QFile>
#include <QtEndian>
#include <array>

namespace Osmo {
static QByteArray box(const QByteArray &tag, const QByteArray &payload)
{
    QByteArray result(4, 0);
    qToBigEndian<quint32>(8 + payload.size(), result.data());
    return result + tag + payload;
}
void tagSpherical(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadWrite)) fail(f.errorString());
    for (qint64 start = 0; start + 8 <= f.size();) {
        if (!f.seek(start)) break;
        auto header = f.read(8);
        if (header.size() != 8) break;
        quint64 size = qFromBigEndian<quint32>(header.constData());
        if (size == 1) {
            auto extended = f.read(8);
            if (extended.size() != 8) break;
            size = qFromBigEndian<quint64>(extended.constData());
        }
        if (size < 8 || size > quint64(f.size() - start)) break;
        if (header.mid(4) != "moov" || size != quint64(f.size() - start)) {
            start += size;
            continue;
        }
        if (size > quint64(INT_MAX) || !f.seek(start)) break;
        auto data = f.read(size);
        auto full = QByteArray(4, 0);
        auto proj = box("proj", box("prhd", full + QByteArray(12, 0)) + box("equi", full + QByteArray(16, 0)));
        auto tags = box("st3d", QByteArray(5, 0)) + box("sv3d", box("svhd", full + QByteArray("KdenCut 360\0", 12)) + proj);
        const std::array<QByteArray, 5> hierarchy{"trak", "mdia", "minf", "stbl", "stsd"};
        std::function<bool(int, int, int)> insert = [&](int parent, int length, int depth) {
            for (int pos = parent + (depth == 5 ? 16 : 8); pos <= parent + length - 8;) {
                auto n = qFromBigEndian<quint32>(data.constData() + pos);
                if (n < 8 || n > quint32(parent + length - pos)) break;
                auto type = data.mid(pos + 4, 4);
                bool changed = false;
                if (depth < 5 && type == hierarchy[depth]) changed = insert(pos, n, depth + 1);
                if (depth == 5 && (type == "avc1" || type == "hev1" || type == "hvc1")) {
                    data.insert(pos + n, tags);
                    qToBigEndian<quint32>(n + tags.size(), data.data() + pos);
                    changed = true;
                }
                if (changed) {
                    qToBigEndian<quint32>(length + tags.size(), data.data() + parent);
                    return true;
                }
                pos += n;
            }
            return false;
        };
        if (data.size() != qint64(size) || !insert(0, size, 0)) break;
        if (!f.seek(start) || f.write(data) != data.size() || !f.flush()) fail(f.errorString());
        return;
    }
    fail("Cannot tag the generated MP4 as spherical video.");
}
} // namespace Osmo
