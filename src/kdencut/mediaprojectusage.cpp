/* SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL */
#include "mediaprojectusage.h"
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "doc/kdenlivedoc.h"
#include <QDateTime>
#include <QDir>
#include <QDomDocument>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTimer>

void KdenCut::trackMediaProjectUsage(QObject *owner)
{
    const QString root = qEnvironmentVariable("KDENCUT_MEDIA_BRIDGE");
    if (root.isEmpty()) return;
    auto *timer = new QTimer(owner);
    QObject::connect(timer, &QTimer::timeout, owner, [root]() {
        // Serialize only on a cleanup request; large bins need not be polled.
        const QString requested = root + QStringLiteral("/usage-request.json");
        QFile request(requested);
        if (!request.open(QIODevice::ReadOnly)) return;
        const auto nonce = QJsonDocument::fromJson(request.readAll()).object().value(QStringLiteral("nonce")).toString();
        request.close();
        QFile::remove(requested);
        const auto *doc = pCore->currentDoc();
        const auto model = pCore->projectItemModel();
        const bool available = doc && model && !doc->loading;
        QDomDocument xml;
        auto clips = xml.createElement(QStringLiteral("clips"));
        xml.appendChild(clips);
        if (available) {
            for (const auto &id : model->getAllClipIds()) {
                if (const auto clip = model->getClipByBinID(id)) clips.appendChild(clip->toXml(xml, false, false));
            }
        }
        QSaveFile output(root + QStringLiteral("/live-usage.json"));
        if (output.open(QIODevice::WriteOnly)) {
            output.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
            output.write(QJsonDocument(QJsonObject{{QStringLiteral("nonce"), nonce}, {QStringLiteral("available"), available},
                {QStringLiteral("at"), QDateTime::currentMSecsSinceEpoch()}, {QStringLiteral("xml"), xml.toString()}}).toJson(QJsonDocument::Compact));
            output.commit();
        }
    });
    timer->start(250);
}
