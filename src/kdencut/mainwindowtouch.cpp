/* SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL */
// MainWindow integration lives here to keep upstream merge conflicts localized.
#include "mainwindow.h"
#include "mobileworkspace.h"
#include "touchmode.h"
#include "mediaimportbridge.h"
#include "mediaprojectusage.h"
#include "mobileediting.h"
#include "core.h"
#include "kdenlivesettings.h"
#include "bin/clipcreator.hpp"
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "dialogs/renderwidget.h"
#include "doc/kdenlivedoc.h"
#include "monitor/monitor.h"
#include "monitor/monitorproxy.h"
#include "project/projectmanager.h"
#include "timeline2/model/timelineitemmodel.hpp"
#include "timeline2/view/timelinetabs.hpp"
#include "timeline2/view/timelinewidget.h"
#include <KActionCollection>
#include <KLocalizedString>
#include <KMessageBox>
#include <KToolBar>
#include <QApplication>
#include <QDialog>
#include <QMenuBar>
#include <QScreen>
#include <QStatusBar>
#include <QTabBar>
#include <QTimer>
#include <QWindow>
#include <QWizard>
#include <kddockwidgets/LayoutSaver.h>

void MainWindow::initializeTouchWorkspace()
{
    KdenCut::trackMediaProjectUsage(this);
    // Watch the screen, rather than the window: a desktop dock layout can impose
    // a window minimum wider than the remote screen after a phone rotates.
    for (QScreen *display : QGuiApplication::screens()) {
        connect(display, &QScreen::availableGeometryChanged, this, [this]() { updateTouchLayout(); });
    }
    connect(KdenCut::TouchMode::instance(), &KdenCut::TouchMode::touchEnabledChanged, this, [this]() { updateTouchLayout(); });
    if (windowHandle()) {
        connect(windowHandle(), &QWindow::screenChanged, this, [this]() { updateTouchLayout(); });
    }
    updateTouchLayout();
}

void MainWindow::connectTouchDock(KDDockWidgets::QtWidgets::DockWidget *dock)
{
    connect(dock, &KDDockWidgets::QtWidgets::DockWidget::isOpenChanged, this, [this, dock](bool open) {
        if (open && m_touchCompactLayout && !m_touchMovingWidgets) {
            QTimer::singleShot(0, this, [this, guard = QPointer<KDDockWidgets::QtWidgets::DockWidget>(dock)]() {
                if (guard && m_touchCompactLayout && !m_touchMovingWidgets) {
                    auto *dock = guard.data();
                    if (dock == m_projectMonitorDock || dock == m_timelineDock) {
                        dock->close();
                    } else {
                        showTouchPanel(dock);
                    }
                }
            });
        }
    });
}

QByteArray MainWindow::layoutForSaving() const
{
    if (m_touchCompactLayout) {
        return m_touchWideLayout;
    }
    KDDockWidgets::LayoutSaver saver(KDDockWidgets::RestoreOption_AbsoluteFloatingDockWindows);
    return saver.serializeLayout();
}

void MainWindow::updateTouchLayout()
{
    QScreen *display = screen();
    if (!display || !m_projectMonitorDock || !m_timelineDock || m_touchMovingWidgets) {
        return;
    }
    // Touch Mode is a complete workspace, independent of the streamed desktop's
    // resolution. A stale browser resolution must never restore desktop docks.
    const bool compact = KdenliveSettings::touchmode();
    if (!compact) {
        leaveTouchLayout();
        return;
    }
    for (QWidget *widget : QApplication::topLevelWidgets()) {
        if (auto *dialog = qobject_cast<QDialog *>(widget); dialog && dialog->isVisible() && !qobject_cast<QWizard *>(dialog)) {
            KdenCut::MobileWorkspace::fitDialog(dialog, display->availableGeometry());
        }
    }
    if (m_touchCompactLayout) {
        return;
    }
    KDDockWidgets::LayoutSaver saver(KDDockWidgets::RestoreOption_RelativeToMainWindow);
    m_touchWideLayout = saver.serializeLayout();
    m_touchWindowState = windowState();
    m_touchCompactLayout = true;
    m_touchMovingWidgets = true;

    // Remove the entire desktop workspace from the window's size calculation.
    // A hidden stack page would still contribute all the dock minimum widths.
    takeCentralWidget()->hide();
    static_cast<QWidget *>(mainDockWindow)->setParent(this);
    for (QWidget *widget : QApplication::allWidgets()) {
        if (auto *dock = qobject_cast<KDDockWidgets::QtWidgets::DockWidget *>(widget)) {
            dock->close();
        }
    }
    m_touchHiddenWidgets.clear();
    const auto bars = findChildren<QToolBar *>();
    for (QWidget *widget : bars) {
        if (!widget->isHidden()) {
            m_touchHiddenWidgets.append(widget);
            widget->hide();
        }
    }
    for (QWidget *widget : {static_cast<QWidget *>(menuBar()), static_cast<QWidget *>(statusBar())}) {
        if (!widget->isHidden()) {
            m_touchHiddenWidgets.append(widget);
            widget->hide();
        }
    }
    for (QWidget *widget : {static_cast<QWidget *>(m_timelineTabs->tabBar()), m_timelineTabs->cornerWidget()}) {
        if (widget && !widget->isHidden()) {
            m_touchHiddenWidgets.append(widget);
            widget->hide();
        }
    }

    // KDDockWidgets returns ownership of the previous guest when replaced.
    // Keep the dock alive with a placeholder while its real monitor is lent.
    m_touchMonitorPlaceholder = new QWidget;
    m_projectMonitorDock->setWidget(m_touchMonitorPlaceholder);
    m_projectMonitor->setMobileLayout(true);
    QMap<QString, QAction *> actions;
    for (auto *action : actionCollection()->actions()) {
        actions.insert(action->objectName(), action);
    }
    m_mobileWorkspace = new KdenCut::MobileWorkspace(m_projectMonitor, m_timelineTabs, actions, this);
    connect(m_mobileWorkspace, &KdenCut::MobileWorkspace::command, this, &MainWindow::touchCommand);
    connect(m_mobileWorkspace, &KdenCut::MobileWorkspace::panelClosed, this, &MainWindow::closeTouchPanel);
    connect(m_projectMonitor, &Monitor::playbackChanged, m_mobileWorkspace, &KdenCut::MobileWorkspace::setPlaying);
    auto *proxy = m_projectMonitor->getControllerProxy();
    connect(proxy, &MonitorProxy::positionChanged, m_mobileWorkspace, [this, proxy]() {
        m_mobileWorkspace->setTimecode(pCore->timecode().getTimecodeFromFrames(proxy->getPosition()));
    });
    setCentralWidget(m_mobileWorkspace);
    setMinimumSize(0, 0);
    m_touchMovingWidgets = false;
    QTimer::singleShot(0, this, [this]() {
        if (m_touchCompactLayout) {
            showFullScreen();
            m_projectMonitor->slotRefreshMonitor(true);
        }
    });
}

void MainWindow::leaveTouchLayout()
{
    if (!m_touchCompactLayout || m_touchMovingWidgets) {
        return;
    }
    m_touchMovingWidgets = true;
    closeTouchPanel();
    m_projectMonitorDock->setWidget(m_projectMonitor);
    delete m_touchMonitorPlaceholder;
    m_touchMonitorPlaceholder = nullptr;
    m_projectMonitor->setMobileLayout(false);
    m_timelineToolBarContainer->layout()->addWidget(m_timelineTabs);
    m_timelineTabs->show();
    takeCentralWidget();
    delete m_mobileWorkspace;
    m_mobileWorkspace = nullptr;
    setCentralWidget(mainDockWindow);
    mainDockWindow->show();
    m_projectMonitor->show();
    m_touchCompactLayout = false;
    KDDockWidgets::LayoutSaver saver(KDDockWidgets::RestoreOption_RelativeToMainWindow);
    saver.restoreLayout(m_touchWideLayout);
    m_touchWideLayout.clear();
    for (const auto &widget : std::as_const(m_touchHiddenWidgets)) {
        if (widget) {
            widget->show();
        }
    }
    m_touchHiddenWidgets.clear();
    setWindowState(m_touchWindowState);
    m_touchMovingWidgets = false;
}

void MainWindow::closeTouchPanel()
{
    if (!m_mobileWorkspace) {
        return;
    }
    QWidget *panel = m_mobileWorkspace->takePanel();
    if (panel && m_touchPanelDock) {
        m_touchPanelDock->setWidget(panel);
        panel->show();
        delete m_touchPanelPlaceholder;
    }
    m_touchPanelPlaceholder = nullptr;
    m_touchPanelDock = nullptr;
}

void MainWindow::showTouchPanel(KDDockWidgets::QtWidgets::DockWidget *dock)
{
    if (!m_mobileWorkspace || !dock || dock == m_projectMonitorDock || dock == m_timelineDock) {
        return;
    }
    if (dock == m_touchPanelDock) {
        dock->close();
        m_mobileWorkspace->showEditor();
        return;
    }
    closeTouchPanel();
    dock->close();
    QWidget *panel = dock->widget();
    if (!panel) {
        return;
    }
    m_touchPanelDock = dock;
    m_touchPanelPlaceholder = new QWidget;
    dock->setWidget(m_touchPanelPlaceholder);
    m_mobileWorkspace->showPanel(panel, dock->title(), dock == m_projectBinDock);
}

void MainWindow::importTouchMedia(bool audio, bool title, bool overlay)
{
    auto *timeline = getCurrentTimeline();
    if (!timeline || !timeline->model()) {
        return;
    }
    closeTouchPanel();
    m_mobileWorkspace->showEditor();
    const auto tracks = timeline->model()->getTracksIds(audio);
    int track = timeline->controller()->activeTrack();
    if (!tracks.contains(track)) {
        track = tracks.isEmpty() ? -1 : tracks.first();
    }
    const bool separateTrack = audio || title || overlay;
    if (track < 0 && !separateTrack) {
        return;
    }
    const auto frame = std::make_shared<int>(m_projectMonitor->position());
    const auto destination = std::make_shared<int>(separateTrack ? -1 : track);
    // Capture the timeline, not the active project: imports finish asynchronously.
    // A closed/switched project must never receive the imported media by accident.
    const auto ready = [target = QPointer<TimelineWidget>(timeline), destination, frame, audio, title, separateTrack](const QString &clipId) {
        if (target && target->model()) {
            const auto candidates = target->model()->getTracksIds(audio);
            if (separateTrack && !candidates.contains(*destination)) {
                // Music and titles are layers, so do not collide with the video's
                // existing picture or sound. Only add a track after import succeeds.
                *destination = -1;
                for (int candidate : candidates) {
                    if (target->model()->getTrackClipsCount(candidate) == 0
                        && target->model()->getTrackProperty(candidate, QStringLiteral("kdenlive:locked")).toInt() == 0) {
                        *destination = candidate;
                        break;
                    }
                }
                const QString trackName = audio ? i18n("Audio") : title ? i18n("Text") : i18n("Overlay");
                if (*destination < 0 && !target->model()->requestTrackInsertion(audio ? 0 : -1, *destination, trackName, audio)) {
                    return;
                }
            } else if (!candidates.contains(*destination)) {
                return;
            }
            const auto inserted = target->controller()->insertClips(*destination, *frame, QStringList(audio ? QStringLiteral("A") + clipId : clipId), true, true);
            for (int id : inserted) {
                *frame = qMax(*frame, target->model()->getClipPosition(id) + target->model()->getClipPlaytime(id));
            }
        }
    };
    const QPointer<KdenliveDoc> document(pCore->currentDoc());
    const QPointer<TimelineWidget> target(timeline);
    const auto serverPicker = [this, document, target, title, ready]() {
        if (!document || document != pCore->currentDoc() || !target || target != getCurrentTimeline()) return;
        pCore->bin()->setReadyCallBack(ready);
        if (auto *action = actionCollection()->action(title ? QStringLiteral("add_text_clip") : QStringLiteral("add_clip"))) action->trigger();
    };
    if (!title && KdenCut::chooseBrowserMedia(this, document, audio ? QStringLiteral("audio") : QStringLiteral("media"),
            [document, target, ready](const QStringList &paths) {
                if (!document || document != pCore->currentDoc() || !target || target != pCore->window()->getCurrentTimeline()) return;
                QList<QUrl> urls;
                for (const auto &path : paths) urls.append(QUrl::fromLocalFile(path));
                // Pass the callback directly: another import cannot steal a pending browser import.
                ClipCreator::createClipsFromList(urls, true, pCore->bin()->getCurrentFolder(), pCore->projectItemModel(), ready);
            }, serverPicker)) return;
    serverPicker();
}

void MainWindow::touchCommand(const QString &name)
{
    if (!m_mobileWorkspace) {
        return;
    }
    if (name == QLatin1String("play")) {
        m_projectMonitor->slotPlay();
    } else if (name == QLatin1String("media")) {
        showTouchPanel(m_projectBinDock);
    } else if (name.startsWith(QLatin1String("ratio-"))) {
        closeTouchPanel();
        KdenCut::setMobileAspect(name.mid(6), this);
    } else if (name.startsWith(QLatin1String("export-"))) {
        closeTouchPanel();
        if (getCurrentTimeline() && pCore->currentDoc()) {
            slotRenderProject();
            if (m_renderWidget) m_renderWidget->startMobileExport(name.mid(7));
        }
    } else if (name == QLatin1String("volume")) {
        if (KdenCut::editMobileVolume(getCurrentTimeline(), this)) showTouchPanel(m_effectStackDock);
    } else if (name == QLatin1String("audio")) {
        importTouchMedia(true);
    } else if (name == QLatin1String("text")) {
        importTouchMedia(false, true);
    } else if (name == QLatin1String("import")) {
        importTouchMedia(false);
    } else if (name == QLatin1String("append")) {
        if (auto *timeline = getCurrentTimeline()) {
            m_projectMonitor->getControllerProxy()->setPosition(timeline->controller()->duration());
            importTouchMedia(false);
        }
    } else if (name == QLatin1String("overlay")) {
        importTouchMedia(false, false, true);
    } else if (name == QLatin1String("effects") || name == QLatin1String("filters")) {
        showTouchPanel(m_effectListDock);
    } else if (name == QLatin1String("adjust")) {
        showTouchPanel(m_effectStackDock);
    } else if (name == QLatin1String("transitions")) {
        showTouchPanel(m_compositionListDock);
    } else if (name == QLatin1String("captions")) {
        closeTouchPanel();
        if (getCurrentTimeline()) slotSpeechRecognition();
    } else if (name == QLatin1String("new")) {
        pCore->projectManager()->newFile(false);
        if (m_mobileWorkspace) {
            m_mobileWorkspace->showEditor();
        }
    } else if (name == QLatin1String("edit")) {
        closeTouchPanel();
        m_mobileWorkspace->showEditTools();
    } else if (name == QLatin1String("insert")) {
        auto *timeline = getCurrentTimeline();
        if (timeline && timeline->model()) {
            QStringList ids;
            for (const QString &id : pCore->bin()->selectedClipsIds(true)) {
                ids.append(id);
            }
            if (!ids.isEmpty()) {
                bool canUseAudio = true;
                bool canUseVideo = true;
                for (const QString &id : std::as_const(ids)) {
                    // Subclips carry /in/out suffixes. Preserve them for insertion
                    // while looking up the source's audio/video capabilities.
                    QString sourceId = id.section(QLatin1Char('/'), 0, 0);
                    const bool audioOnly = sourceId.startsWith(QLatin1Char('A'));
                    const bool videoOnly = sourceId.startsWith(QLatin1Char('V'));
                    if (audioOnly || videoOnly) {
                        sourceId.remove(0, 1);
                    }
                    const auto clip = pCore->projectItemModel()->getClipByBinID(sourceId);
                    if (!clip) {
                        KMessageBox::information(this, i18n("The selected media is no longer available."));
                        return;
                    }
                    canUseAudio = canUseAudio && !videoOnly && clip->hasAudio();
                    canUseVideo = canUseVideo && !audioOnly && clip->hasVideo();
                }
                if (!canUseAudio && !canUseVideo) {
                    KMessageBox::information(this, i18n("Add audio clips and video clips separately so each can use a compatible track."));
                    return;
                }
                const auto model = timeline->model();
                int track = timeline->controller()->activeTrack();
                const bool validTrack = model->isTrack(track);
                const bool audioTrack = canUseAudio && (!canUseVideo || (validTrack && model->isAudioTrack(track)));
                const auto candidates = model->getTracksIds(audioTrack);
                const auto usable = [&model, &candidates](int candidate) {
                    return candidates.contains(candidate) && model->getTrackProperty(candidate, QStringLiteral("kdenlive:locked")).toInt() == 0;
                };
                if (!usable(track)) {
                    track = -1;
                    for (int candidate : candidates) {
                        if (usable(candidate)) {
                            track = candidate;
                            break;
                        }
                    }
                    if (track < 0 && !model->requestTrackInsertion(audioTrack ? 0 : -1, track, audioTrack ? i18n("Audio") : i18n("Video"), audioTrack)) {
                        KMessageBox::information(this, i18n("Could not create a track for the selected media."));
                        return;
                    }
                }
                const auto inserted = timeline->controller()->insertClips(track, m_projectMonitor->position(), ids, true, true);
                if (!inserted.isEmpty()) {
                    timeline->controller()->setActiveTrack(track);
                    closeTouchPanel();
                } else {
                    KMessageBox::information(this, i18n("The selected media could not be added at the playhead. Try an empty part of the timeline."));
                }
            }
        }
    } else if (name == QLatin1String("file_open")) {
        m_mobileWorkspace->showEditor();
    }
}
