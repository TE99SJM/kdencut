// SPDX-License-Identifier: GPL-3.0-or-later
#include "osmo360import.h"

#include <KLocalizedString>
#include <KMessageBox>
#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMainWindow>
#include <QProcess>
#include <QProgressBar>
#include <QQueue>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTimer>
#include <optional>

namespace {
struct Import {
    QString source;
    std::function<bool()> valid;
    std::function<void(const QString &)> imported;
};

class ImportQueue : public QObject
{
public:
    explicit ImportQueue(QMainWindow *window)
        : QObject(window)
        , m_window(window)
        , m_bar(new QProgressBar(window))
    {
        setObjectName(QStringLiteral("osmo360ImportQueue"));
        m_bar->setAccessibleName(i18n("Osmo 360 import progress"));
        window->statusBar()->addPermanentWidget(m_bar);
        m_bar->hide();
        connect(&m_child, &QProcess::readyReadStandardError, this, [this] { readStatus(); });
        connect(&m_child, &QProcess::errorOccurred, this, [this](QProcess::ProcessError err) {
            if (err == QProcess::FailedToStart) finish({}, m_child.errorString());
        });
        connect(&m_child, &QProcess::finished, this, [this](int code, QProcess::ExitStatus exit) {
            readStatus();
            if (exit == QProcess::NormalExit && code == 2) {
                finish({}, {});
                return;
            }
            const auto json = QJsonDocument::fromJson(m_child.readAllStandardOutput()).object();
            const QFileInfo file(json.value(QStringLiteral("clip")).toString());
            if (exit == QProcess::NormalExit && code == 0 && file.isAbsolute() && file.suffix().compare(QStringLiteral("mp4"), Qt::CaseInsensitive) == 0 && file.isFile()) {
                finish(file.absoluteFilePath(), {});
                return;
            }
            const auto error = QString::fromUtf8(m_errors + m_partial).trimmed();
            finish({}, error.isEmpty() ? i18n("The 360 editor did not return a completed clip.") : error);
        });
    }
    ~ImportQueue() override { m_child.disconnect(this); }

    void add(Import job)
    {
        m_waiting.enqueue(std::move(job));
        // Defer even the first launch until the current bin import/undo group has returned.
        QTimer::singleShot(0, this, [this] { startNext(); });
    }

private:
    void startNext()
    {
        while (!m_active && !m_waiting.isEmpty()) {
            auto job = m_waiting.dequeue();
            if (!job.valid()) continue;
            m_active = std::move(job);
            m_errors.clear();
            m_partial.clear();
            progress(i18n("Opening 360 clip"), -1);
            const auto editor = QStandardPaths::findExecutable(QStringLiteral("osmo360-editor"), {QCoreApplication::applicationDirPath()});
            if (editor.isEmpty()) {
                finish({}, i18n("The native Osmo 360 editor is missing from the KdenCut installation."));
                return;
            }
            m_child.start(editor, {QStringLiteral("--integrated"), m_active->source});
        }
    }

    void progress(const QString &stage, int percent)
    {
        m_bar->setFormat(percent < 0 ? stage : stage + QStringLiteral(" %p%"));
        m_bar->setToolTip(stage + QStringLiteral(" — ") + QFileInfo(m_active->source).fileName());
        m_bar->setRange(0, percent < 0 ? 0 : 100);
        if (percent >= 0) m_bar->setValue(percent);
        m_bar->show();
    }

    void readStatus()
    {
        m_partial += m_child.readAllStandardError();
        qsizetype end;
        while ((end = m_partial.indexOf('\n')) >= 0) {
            const QByteArray line = m_partial.left(end).trimmed();
            m_partial.remove(0, end + 1);
            const auto fields = line.split(' ');
            if (!m_active || fields.size() != 3 || fields[0] != "osmo360") {
                m_errors = (m_errors + line + '\n').right(1200);
                continue;
            }
            bool ok = false;
            const int value = fields[2].toInt(&ok);
            if (!ok || value < -1 || value > 100) continue;
            if (fields[1] == "open") progress(i18n("Opening 360 clip"), value);
            else if (fields[1] == "preview") progress(i18n("Loading 360 preview"), value);
            else if (fields[1] == "proxy") progress(i18n("Preparing 360 playback"), value);
            else if (fields[1] == "render") progress(i18n("Exporting 360 clip"), value);
            else if (fields[1] == "idle") m_bar->hide();
        }
        m_partial = m_partial.right(4096);
    }

    void finish(QString path, const QString &error)
    {
        if (!m_active) return;
        m_bar->hide();
        if (m_active->valid() && !error.isEmpty()) {
            if (m_active->source.endsWith(QStringLiteral(".osv"), Qt::CaseInsensitive)) {
                if (KMessageBox::warningTwoActions(m_window, error, i18n("Osmo 360 import"), KGuiItem(i18n("Import separate lenses")), KStandardGuiItem::cancel()) == KMessageBox::PrimaryAction)
                    path = m_active->source;
            } else {
                KMessageBox::error(m_window, error, i18n("Osmo 360 import"));
            }
        }
        if (!path.isEmpty() && m_active->valid()) m_active->imported(path);
        m_active.reset();
        QTimer::singleShot(0, this, [this] { startNext(); });
    }

    QMainWindow *m_window;
    QProgressBar *m_bar;
    QProcess m_child;
    QQueue<Import> m_waiting;
    std::optional<Import> m_active;
    QByteArray m_partial, m_errors;
};
}

void KdenCut::queueOsmo360Clip(QMainWindow *window, const QString &source, std::function<bool()> valid, std::function<void(const QString &)> imported)
{
    auto *queue = static_cast<ImportQueue *>(window->findChild<QObject *>(QStringLiteral("osmo360ImportQueue"), Qt::FindDirectChildrenOnly));
    if (!queue) queue = new ImportQueue(window);
    queue->add({source, std::move(valid), std::move(imported)});
}
