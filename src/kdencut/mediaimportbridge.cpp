/* SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL */
#include "mediaimportbridge.h"
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QSaveFile>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>

namespace KdenCut {
bool chooseBrowserMedia(QWidget *parent, QObject *project, const QString &kind,
                        const std::function<void(const QStringList &)> &selected,
                        const std::function<void()> &serverPicker)
{
    const QString root = qEnvironmentVariable("KDENCUT_MEDIA_BRIDGE");
    if (root.isEmpty() || !QDir().mkpath(root)) return false;
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString requestPath = root + QStringLiteral("/request.json");
    const QString responsePath = root + QLatin1Char('/') + id + QStringLiteral(".json");
    const QJsonObject request{{QStringLiteral("id"), id}, {QStringLiteral("kind"), kind},
                              {QStringLiteral("created"), QDateTime::currentMSecsSinceEpoch()}};
    QSaveFile file(requestPath);
    if (!file.open(QIODevice::WriteOnly)) return false;
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    file.write(QJsonDocument(request).toJson(QJsonDocument::Compact));
    if (!file.commit()) return false;
    auto *dialog = new QDialog(parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(QObject::tr("Add media"));
    dialog->setWindowModality(Qt::WindowModal);
    auto *layout = new QVBoxLayout(dialog);
    auto *label = new QLabel(QObject::tr("Choose Photos / Files, server storage, or a NAS in your browser.\nKeep this project open while files transfer."));
    label->setWordWrap(true);
    layout->addWidget(label);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel);
    auto *local = buttons->addButton(QObject::tr("Server file picker"), QDialogButtonBox::ActionRole);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    QObject::connect(local, &QPushButton::clicked, dialog, [dialog, serverPicker]() {
        dialog->reject();
        serverPicker();
    });
    const QPointer<QObject> target(project);
    QObject::connect(project, &QObject::destroyed, dialog, &QDialog::reject);
    QObject::connect(dialog, &QDialog::finished, dialog, [requestPath, responsePath, id]() {
        QFile current(requestPath);
        if (current.open(QIODevice::ReadOnly) && QJsonDocument::fromJson(current.readAll()).object().value(QStringLiteral("id")).toString() == id) {
            current.close();
            QFile::remove(requestPath);
        }
        QFile::remove(responsePath);
    });
    auto *timer = new QTimer(dialog);
    QObject::connect(timer, &QTimer::timeout, dialog, [dialog, target, responsePath, id, selected, serverPicker]() {
        QFile response(responsePath);
        if (!response.open(QIODevice::ReadOnly)) return;
        const auto result = QJsonDocument::fromJson(response.readAll()).object();
        if (result.value(QStringLiteral("id")).toString() != id) return;
        const auto status = result.value(QStringLiteral("status")).toString();
        QStringList paths;
        for (const auto &value : result.value(QStringLiteral("paths")).toArray()) {
            const QFileInfo info(value.toString());
            if (info.isAbsolute() && info.isFile() && info.isReadable()) paths.append(info.canonicalFilePath());
        }
        dialog->accept();
        if (!target) return;
        if (status == QLatin1String("ready") && !paths.isEmpty()) selected(paths);
        else if (status == QLatin1String("server")) serverPicker();
    });
    timer->start(400);
    dialog->resize(320, 160);
    dialog->open();
    return true;
}
}
