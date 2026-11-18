/* SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL */
#include "../../../src/kdencut/phonepresets.h"
#include <QDomDocument>
#include <QFile>
#include <QtTest>

class PhonePresets : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void aspectRatios()
    {
        QCOMPARE(KdenCut::phoneAspect("portrait"), QSize(1080, 1920));
        QCOMPARE(KdenCut::phoneAspect("square"), QSize(1080, 1080));
        QCOMPARE(KdenCut::phoneAspect("landscape"), QSize(1920, 1080));
        QVERIFY(!KdenCut::phoneAspect("invalid").isValid());
    }
    void installedExportPresets()
    {
        QFile file(QStringLiteral(KDENCUT_SOURCE_DIR "/data/profiles.xml"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QDomDocument xml;
        QVERIFY(xml.setContent(&file));
        QMap<QString, QString> presets;
        const auto nodes = xml.elementsByTagName(QStringLiteral("profile"));
        for (int i = 0; i < nodes.size(); ++i) {
            const auto el = nodes.at(i).toElement();
            presets.insert(el.attribute("name"), el.attribute("args"));
        }
        for (const QString platform : {QStringLiteral("TikTok"), QStringLiteral("Reels"), QStringLiteral("Shorts")}) {
            for (const QString codec : {QStringLiteral("libx264"), QStringLiteral("h264_nvenc"), QStringLiteral("h264_vaapi")}) {
                const auto preset = KdenCut::socialPreset(platform, codec);
                QVERIFY2(presets.contains(preset), qPrintable(preset));
                const QString args = presets.value(preset);
                QVERIFY(args.contains("s=1080x1920"));
                QVERIFY(args.contains("vcodec=" + codec));
                QVERIFY(args.contains("acodec=aac"));
                QVERIFY(args.contains("progressive=1"));
                QVERIFY(args.contains("movflags=+faststart"));
            }
            QCOMPARE(KdenCut::socialPreset(platform, "missing-driver"), KdenCut::socialPreset(platform, "libx264"));
        }
        QVERIFY(KdenCut::socialPreset("invalid", "libx264").isEmpty());
    }
};
QTEST_GUILESS_MAIN(PhonePresets)
#include "check_phone_presets.moc"
