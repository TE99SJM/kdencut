/* SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL */
#include "../../../src/kdencut/mediaimportbridge.h"
#include <QApplication>
#include <QDialog>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QtTest>

class MediaBridgeChecks : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void completedFilesOnly()
    {
        QTemporaryDir dir;
        qputenv("KDENCUT_MEDIA_BRIDGE", dir.path().toUtf8());
        QWidget parent;
        QObject project;
        QStringList result;
        QVERIFY(KdenCut::chooseBrowserMedia(&parent, &project, "media", [&](const QStringList &paths) { result = paths; }, []() {}));
        QFile request(dir.filePath("request.json"));
        QVERIFY(request.open(QIODevice::ReadOnly));
        const QString id = QJsonDocument::fromJson(request.readAll()).object().value("id").toString();
        QVERIFY(!id.isEmpty());
        QFile clip(dir.filePath("clip.mov"));
        QVERIFY(clip.open(QIODevice::WriteOnly)); clip.write("media"); clip.close();
        QFile response(dir.filePath(id + ".json"));
        QVERIFY(response.open(QIODevice::WriteOnly));
        response.write(QJsonDocument(QJsonObject{{"id", "wrong"}, {"status", "ready"}, {"paths", QJsonArray{clip.fileName()}}}).toJson()); response.close();
        QTest::qWait(500); QVERIFY(result.isEmpty());
        QVERIFY(response.open(QIODevice::WriteOnly | QIODevice::Truncate));
        response.write(QJsonDocument(QJsonObject{{"id", id}, {"status", "ready"}, {"paths", QJsonArray{clip.fileName(), dir.filePath("missing.mov")}}}).toJson()); response.close();
        QTRY_COMPARE(result, QStringList{clip.fileName()});
        QVERIFY(!QFile::exists(dir.filePath("request.json")));
        QVERIFY(!QFile::exists(response.fileName()));
    }
    void closedProjectCancels()
    {
        QTemporaryDir dir; qputenv("KDENCUT_MEDIA_BRIDGE", dir.path().toUtf8());
        QWidget parent;
        auto *project = new QObject;
        int calls = 0;
        QVERIFY(KdenCut::chooseBrowserMedia(&parent, project, "audio", [&](const QStringList &) { calls++; }, [&]() { calls++; }));
        delete project;
        QTest::qWait(500);
        QVERIFY(!QFile::exists(dir.filePath("request.json")));
        QCOMPARE(calls, 0);
    }
    void desktopWithoutBridgeFallsBack()
    {
        qunsetenv("KDENCUT_MEDIA_BRIDGE"); QWidget parent; QObject project;
        QVERIFY(!KdenCut::chooseBrowserMedia(&parent, &project, "media", [](const QStringList &) {}, []() {}));
    }
};
QTEST_MAIN(MediaBridgeChecks)
#include "check_media_bridge.moc"
