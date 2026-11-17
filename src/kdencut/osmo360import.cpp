// SPDX-License-Identifier: GPL-3.0-or-later
#include "osmo360import.h"

#include <KLocalizedString>
#include <KMessageBox>
#include <QEventLoop>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>

QString KdenCut::prepareOsmo360Clip(const QString &source, QWidget *parent)
{
    const auto script = QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("scripts/osmo360/editor.py"));
    auto python = QStandardPaths::findExecutable(QStringLiteral("python3"));
    if (python.isEmpty()) {
        python = QStandardPaths::findExecutable(QStringLiteral("python"));
    }
    QString error;
    if (script.isEmpty() || python.isEmpty()) {
        error = i18n("The Osmo 360 editor or Python is missing.");
    } else {
        QProcess child;
        QEventLoop loop;
        QObject::connect(&child, &QProcess::finished, &loop, &QEventLoop::quit);
        child.start(python, {script, QStringLiteral("--integrated"), source});
        if (child.waitForStarted()) {
            // The separate editor owns the interaction. The parent can repaint while
            // the current import's bin folder, callback and undo group stay intact.
            if (child.state() != QProcess::NotRunning) {
                loop.exec(QEventLoop::ExcludeUserInputEvents);
            }
            if (child.exitStatus() == QProcess::NormalExit && child.exitCode() == 2) {
                return {};
            }
            const auto result = QJsonDocument::fromJson(child.readAllStandardOutput()).object();
            const QFileInfo rendered(result.value(QStringLiteral("clip")).toString());
            if (child.exitStatus() == QProcess::NormalExit && child.exitCode() == 0 && rendered.isAbsolute() && rendered.isFile()
                && rendered.suffix().compare(QStringLiteral("mp4"), Qt::CaseInsensitive) == 0) {
                return rendered.absoluteFilePath();
            }
            error = QString::fromUtf8(child.readAllStandardError()).right(1200).trimmed();
            if (error.isEmpty()) {
                error = i18n("The 360 editor did not return a completed clip.");
            }
        } else {
            error = child.errorString();
        }
    }
    if (source.endsWith(QStringLiteral(".osv"), Qt::CaseInsensitive)) {
        if (KMessageBox::warningTwoActions(parent, error, i18n("Osmo 360 import"), KGuiItem(i18n("Import separate lenses")), KStandardGuiItem::cancel())
            == KMessageBox::PrimaryAction) {
            return source;
        }
    } else {
        KMessageBox::error(parent, error, i18n("Osmo 360 import"));
    }
    return {};
}
