// SPDX-License-Identifier: GPL-3.0-or-later
#include "osmo.h"
#include <QCoreApplication>
#include <QLockFile>
#include <QSaveFile>
#include <QStandardPaths>
namespace Osmo {
void installJob(const QString &root)
{
    QDir dir(root.isEmpty() ? QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)).filePath("kdenlive") : root);
    if (!dir.mkpath(".")) fail("Cannot create the Kdenlive user data directory.");
    QString path = dir.filePath("clipjobsettings.rc");
    QLockFile lock(path + ".osmo.lock");
    if (!lock.tryLock(1000)) fail("Another add-on installer is running.");
    QFile f(path);
    QStringList lines;
    if (f.exists()) {
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) fail(f.errorString());
        lines = QString::fromUtf8(f.readAll()).split('\n');
        f.close();
    }
    QMap<QString, QString> records{{"Ids", "Osmo 360 Editor"},
                                   {"Binary", QCoreApplication::applicationFilePath()},
                                   {"Parameters", "{source} --output {output}"},
                                   {"Output", "osmo360.mp4"},
                                   {"FolderUse", "subfolder"},
                                   {"FolderName", "360 edits"},
                                   {"EnabledTypes", "v"}};
    for (auto it = records.begin(); it != records.end(); ++it) {
        QString group = "[" + it.key() + "]", key = "osmo360reframe", entry = key + "=" + it.value();
        int start = lines.indexOf(group);
        if (start < 0) {
            lines << group << entry << "";
            continue;
        }
        int i = start + 1;
        for (; i < lines.size() && !lines[i].startsWith('['); ++i)
            if (lines[i].section('=', 0, 0).trimmed() == key) break;
        if (i < lines.size() && lines[i].section('=', 0, 0).trimmed() == key)
            lines[i] = entry;
        else
            lines.insert(start + 1, entry);
    }
    if (!lines.isEmpty() && !lines.last().isEmpty()) lines << QString();
    QSaveFile out(path);
    auto bytes = lines.join('\n').toUtf8();
    if (!out.open(QIODevice::WriteOnly) || out.write(bytes) != bytes.size() || !out.commit()) fail(out.errorString());
}
} // namespace Osmo
