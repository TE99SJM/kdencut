/* SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL */
#include "../../../src/kdencut/mobileworkspace.h"
#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalSpy>
#include <QSplitter>
#include <QToolButton>
#include <QPushButton>
#include <QPainter>
#include <QQuickItem>
#include <QQuickWidget>
#include <QWindow>
#include <QQuickImageProvider>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QStandardItemModel>
#include <QVBoxLayout>
#include <QtTest>
#ifdef CHECK_KDDOCK
#include <kddockwidgets/DockWidget.h>
#include <kddockwidgets/MainWindow.h>
#include <kddockwidgets/LayoutSaver.h>
#include <kddockwidgets/Config.h>
#endif

static QImage sampleFrame()
{
    QImage frame(270, 480, QImage::Format_RGB32);
    QPainter p(&frame);
    QLinearGradient sky(0, 0, 0, 480);
    sky.setColorAt(0, QColor("#477a8c"));
    sky.setColorAt(0.5, QColor("#c8c6a7"));
    sky.setColorAt(1, QColor("#385947"));
    p.fillRect(frame.rect(), sky);
    p.setPen(QColor("#e3e9de"));
    p.drawText(frame.rect(), Qt::AlignCenter, QStringLiteral("PREVIEW SAMPLE"));
    return frame;
}

class SampleThumbnails : public QQuickImageProvider
{
public:
    SampleThumbnails() : QQuickImageProvider(QQuickImageProvider::Image) {}
    QImage requestImage(const QString &, QSize *size, const QSize &) override
    {
        const QImage frame = sampleFrame();
        if (size) *size = frame.size();
        return frame;
    }
};

class SamplePreview : public QWidget
{
public:
    using QWidget::QWidget;
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.fillRect(rect(), QColor("#121212"));
        const QImage frame = sampleFrame();
        const QSize fitted = frame.size().scaled(size() - QSize(20, 12), Qt::KeepAspectRatio);
        const QRect target(QPoint((width() - fitted.width()) / 2, (height() - fitted.height()) / 2), fitted);
        p.drawImage(target, frame);
    }
};

static QQuickItem *visualItem(QQuickItem *root, const QString &name)
{
    if (root->objectName() == name) return root;
    for (auto *child : root->childItems()) {
        if (auto *found = visualItem(child, name)) return found;
    }
    return nullptr;
}

class MobileChecks : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void toolSwipe()
    {
        KdenCut::MobileWorkspace workspace(new QLabel("Preview"), new QLabel("Timeline"), {});
        workspace.resize(430, 876);
        workspace.show();
        QTest::qWait(100);
        auto *edit = workspace.findChild<QToolButton *>(QStringLiteral("mobile_edit"));
        QScrollArea *strip = nullptr;
        for (auto *area : workspace.findChildren<QScrollArea *>()) {
            if (area->isAncestorOf(edit)) { strip = area; break; }
        }
        QVERIFY(strip);
        QVERIFY(strip->horizontalScrollBar()->maximum() > 0);
        const QPoint start = strip->viewport()->mapTo(&workspace, QPoint(350, 30));
        auto *target = workspace.childAt(start);
        QVERIFY(target);
        QSignalSpy commands(&workspace, &KdenCut::MobileWorkspace::command);
        QTest::mousePress(workspace.windowHandle(), Qt::LeftButton, Qt::NoModifier, start);
        for (int x = 340; x >= 90; x -= 10) {
            const QPoint pos = strip->viewport()->mapTo(&workspace, QPoint(x, 30));
            QTest::mouseMove(workspace.windowHandle(), pos, 15);
            // QTest's delay advances event timestamps, not the wall clock.
            // QScroller's gesture recognizer uses a monotonic clock, so allow
            // actual animation frames instead of injecting a zero-time flick.
            QTest::qWait(16);
        }
        const QPoint end = strip->viewport()->mapTo(&workspace, QPoint(90, 30));
        QTest::mouseRelease(workspace.windowHandle(), Qt::LeftButton, Qt::NoModifier, end);
        QTRY_VERIFY_WITH_TIMEOUT(strip->horizontalScrollBar()->value() > 50, 2000);
        QVERIFY(commands.isEmpty());
    }

    void scrollingTimeline()
    {
        QStandardItemModel tracks;
        QHash<int, QByteArray> roles;
        const QList<QByteArray> names{"item", "audio", "locked", "start", "duration", "name", "clipType", "selected", "in", "speed", "clipThumbId"};
        for (int i = 0; i < names.size(); ++i) roles.insert(Qt::UserRole + i, names.at(i));
        tracks.setItemRoleNames(roles);
        auto makeItem = [&](const QVariantList &values) {
            auto *item = new QStandardItem;
            for (int i = 0; i < values.size(); ++i) item->setData(values.at(i), Qt::UserRole + i);
            return item;
        };
        auto *videoTrack = makeItem({10, false, false, 0, 240, "Video", 14, false, 0, 1., ""});
        auto *video = makeItem({101, false, false, 0, 240, "Example video", 2, false, 0, 1., "1/#"});
        videoTrack->appendRow(video);
        tracks.appendRow(videoTrack);
        auto *audioTrack = makeItem({20, true, false, 0, 150, "Audio", 14, false, 0, 1., ""});
        audioTrack->appendRow(makeItem({102, true, false, 30, 120, "Background audio", 1, false, 0, 1., ""}));
        tracks.appendRow(audioTrack);
        auto *view = new QQuickWidget;
        view->setResizeMode(QQuickWidget::SizeRootObjectToView);
        view->engine()->addImageProvider(QStringLiteral("thumbnail"), new SampleThumbnails);
        QSignalSpy warnings(view->engine(), &QQmlEngine::warnings);
        const QUrl source = QUrl::fromLocalFile(QStringLiteral(KDENCUT_SOURCE_DIR "/src/timeline2/view/qml/TouchTimeline.qml"));
        auto *component = new QQmlComponent(view->engine(), source, view);
        auto *content = component->createWithInitialProperties({{"tracks", QVariant::fromValue(&tracks)}, {"subtitles", QVariantList()},
                                                               {"frame", 0}, {"duration", 240}, {"fps", 30.}, {"pixelsPerFrame", 2.}, {"compositionType", 13}});
        QVERIFY2(content, qPrintable(component->errorString()));
        view->setContent(source, component, content);
        QCOMPARE(view->status(), QQuickWidget::Ready);
        QMap<QString, QAction *> actions;
        for (const auto &id : {"file_save", "project_render", "edit_undo", "edit_redo", "cut_timeline_clip", "delete_timeline_selection", "duplicate_timeline_clip", "zoom_fit", "view_zoom_in", "view_zoom_out"}) {
            actions.insert(QString::fromLatin1(id), new QAction(this));
        }
        auto *preview = new SamplePreview;
        preview->setMinimumSize(160, 90);
        KdenCut::MobileWorkspace workspace(preview, view, actions);
        workspace.resize(430, 876);
        workspace.show();
        QTest::qWait(80);
        auto *root = view->rootObject();
        auto *canvas = visualItem(root, QStringLiteral("touchTimelineScroll"));
        QVERIFY(canvas);
        auto *playhead = visualItem(root, QStringLiteral("touchPlayhead"));
        QVERIFY(playhead);
        QCOMPARE(playhead->x() + 1, view->width() / 2.);
        QSignalSpy seeks(root, SIGNAL(seekRequested(int)));
        QSignalSpy selects(root, SIGNAL(selectRequested(int,int,bool)));
        QSignalSpy moves(root, SIGNAL(moveRequested(int,int,int,bool)));
        QSignalSpy trims(root, SIGNAL(trimRequested(int,int,bool)));
        QVERIFY(seeks.isValid() && selects.isValid() && moves.isValid() && trims.isValid());

        // A swipe over a clip scrolls and seeks; it must not edit the project.
        QTest::mousePress(view, Qt::LeftButton, Qt::NoModifier, QPoint(350, 88));
        for (int x = 340; x >= 150; x -= 10) QTest::mouseMove(view, QPoint(x, 88), 15);
        QTest::mouseRelease(view, Qt::LeftButton, Qt::NoModifier, QPoint(150, 88));
        QTest::qWait(150);
        QVERIFY(canvas->property("contentX").toReal() > 50);
        QVERIFY(!seeks.isEmpty());
        QVERIFY(moves.isEmpty());
        QCOMPARE(playhead->x() + 1, view->width() / 2.);
        QMetaObject::invokeMethod(canvas, "cancelFlick");
        canvas->setProperty("contentX", 0);
        QTest::qWait(30);

        QTest::mouseClick(view, Qt::LeftButton, Qt::NoModifier, QPoint(290, 88));
        QVERIFY(!selects.isEmpty());
        QCOMPARE(selects.last().at(0).toInt(), 101);
        video->setData(true, Qt::UserRole + names.indexOf("selected"));
        QTest::qWait(30);

        // Reordering requires a deliberate hold, and commits only on release.
        QTest::mousePress(view, Qt::LeftButton, Qt::NoModifier, QPoint(290, 88));
        QTest::qWait(520);
        QVERIFY(root->property("editingClip").toBool());
        QTest::mouseMove(view, QPoint(350, 88), 30);
        QVERIFY(moves.isEmpty());
        QTest::mouseRelease(view, Qt::LeftButton, Qt::NoModifier, QPoint(350, 88));
        QCOMPARE(moves.size(), 1);
        QCOMPARE(moves.first().at(0).toInt(), 101);
        QCOMPARE(moves.first().at(2).toInt(), 30);
        QVERIFY(!root->property("editingClip").toBool());

        QTest::mousePress(view, Qt::LeftButton, Qt::NoModifier, QPoint(215, 88));
        QTest::mouseMove(view, QPoint(245, 88), 30);
        QVERIFY(trims.isEmpty());
        QTest::mouseRelease(view, Qt::LeftButton, Qt::NoModifier, QPoint(245, 88));
        QCOMPARE(trims.size(), 1);
        QCOMPARE(trims.first().at(0).toInt(), 101);
        QCOMPARE(trims.first().at(1).toInt(), 225);
        QCOMPARE(trims.first().at(2).toBool(), false);
        QVERIFY(!root->property("editingClip").toBool());

        video->setData(false, Qt::UserRole + names.indexOf("selected"));
        auto *thumbs = visualItem(root, QStringLiteral("touchThumbnails"));
        QVERIFY(thumbs);
        QVERIFY(thumbs->property("count").toInt() > 0);
        QVERIFY(thumbs->property("count").toInt() <= 9);
        video->setData(1000, Qt::UserRole + names.indexOf("start"));
        QTRY_COMPARE(thumbs->property("count").toInt(), 0);
        video->setData(0, Qt::UserRole + names.indexOf("start"));
        QTRY_VERIFY(thumbs->property("count").toInt() > 0);
        QTest::qWait(50);
        if (const auto screenshot = qEnvironmentVariable("KDENCUT_TOUCH_SCREENSHOT"); !screenshot.isEmpty()) {
            QVERIFY(workspace.grab().save(screenshot));
        }
        QVERIFY(warnings.isEmpty());
    }
#ifdef CHECK_KDDOCK
    void dockHandoff()
    {
        KDDockWidgets::initFrontend(KDDockWidgets::FrontendType::QtWidgets);
        KDDockWidgets::QtWidgets::MainWindow desktop(QStringLiteral("desktop"));
        auto *dock = new KDDockWidgets::QtWidgets::DockWidget(QStringLiteral("project_monitor"));
        QPointer<QLabel> preview = new QLabel(QStringLiteral("Preview"));
        dock->setWidget(preview);
        desktop.addDockWidget(dock, KDDockWidgets::Location_OnTop);
        desktop.resize(1280, 720);
        desktop.show();
        QTest::qWait(20);
        KDDockWidgets::LayoutSaver saver(KDDockWidgets::RestoreOption_RelativeToMainWindow);
        const QByteArray saved = saver.serializeLayout();
        desktop.hide();
        QWidget owner;
        static_cast<QWidget *>(&desktop)->setParent(&owner);
        QCOMPARE(desktop.parentWidget(), &owner);
        static_cast<QWidget *>(&desktop)->setParent(nullptr);
        dock->close();
        auto *placeholder = new QWidget;
        dock->setWidget(placeholder);
        auto *mobile = new KdenCut::MobileWorkspace(preview, new QLabel(QStringLiteral("Timeline")), {});
        mobile->resize(430, 820);
        mobile->show();
        QTest::qWait(20);
        QVERIFY(preview->isVisible());
        QVERIFY(!dock->isOpen());
        dock->setWidget(preview);
        preview->show();
        delete placeholder;
        delete mobile;
        QVERIFY(preview);
        QVERIFY(saver.restoreLayout(saved));
        desktop.show();
        QTest::qWait(20);
        QVERIFY(preview->isVisible());
        QCOMPARE(dock->widget(), preview.data());
        delete dock;
        QVERIFY(!preview);
    }
#endif

    void legacyDialogRemainsUsable()
    {
        QDialog dialog;
        auto *layout = new QVBoxLayout(&dialog);
        auto *form = new QLabel(QStringLiteral("Large export form"));
        form->setMinimumSize(900, 700);
        layout->addWidget(form);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        layout->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        dialog.show();
        KdenCut::MobileWorkspace::fitDialog(&dialog, QRect(0, 0, 430, 820));
        QTest::qWait(20);
        QVERIFY(dialog.width() <= 430);
        QVERIFY(dialog.height() <= 820);
        auto *scroll = dialog.findChild<QScrollArea *>();
        QVERIFY(scroll);
        QVERIFY(scroll->horizontalScrollBar()->maximum() > 0);
        QVERIFY(scroll->verticalScrollBar()->maximum() > 0);
        KdenCut::MobileWorkspace::fitDialog(&dialog, QRect(0, 0, 320, 568));
        QTest::qWait(20);
        QVERIFY(dialog.width() <= 320);
        QVERIFY(dialog.height() <= 568);
        QCOMPARE(dialog.findChildren<QScrollArea *>().size(), 1);
        buttons->button(QDialogButtonBox::Ok)->click();
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
    }

    void viewportAndActions()
    {
        QMap<QString, QAction *> actions;
        for (const QString &id : {QStringLiteral("file_save"), QStringLiteral("project_render"), QStringLiteral("edit_undo"),
                                  QStringLiteral("edit_redo"), QStringLiteral("cut_timeline_clip"), QStringLiteral("delete_timeline_selection"),
                                  QStringLiteral("duplicate_timeline_clip"), QStringLiteral("zoom_fit"), QStringLiteral("file_open"),
                                  QStringLiteral("view_zoom_in"), QStringLiteral("view_zoom_out"),
                                  QStringLiteral("kdencut_touch_mode")}) {
            actions.insert(id, new QAction(id, this));
        }
        auto *preview = new QLabel(QStringLiteral("Project preview"));
        preview->setMinimumSize(160, 90);
        preview->setAlignment(Qt::AlignCenter);
        auto *timeline = new QLabel(QStringLiteral("Timeline"));
        KdenCut::MobileWorkspace workspace(preview, timeline, actions);
        workspace.show();
        for (const QSize &size : {QSize(320, 568), QSize(375, 667), QSize(430, 876), QSize(932, 374), QSize(1280, 720), QSize(430, 344), QSize(430, 876)}) {
            workspace.resize(size);
            QTest::qWait(20);
            QCOMPARE(workspace.size(), size);
            auto *exportButton = workspace.findChild<QToolButton *>(QStringLiteral("mobileExport"));
            const QRect exportRect(exportButton->mapTo(&workspace, QPoint()), exportButton->size());
            QVERIFY(workspace.rect().contains(exportRect));
            QVERIFY(preview->isVisible());
            QVERIFY(timeline->isVisible());
            QVERIFY(workspace.rect().contains(QRect(preview->mapTo(&workspace, QPoint()), preview->size())));
            QVERIFY(workspace.rect().contains(QRect(timeline->mapTo(&workspace, QPoint()), timeline->size())));
            auto *splitter = workspace.findChild<QSplitter *>();
            QCOMPARE(splitter->orientation(), size.width() > size.height() ? Qt::Horizontal : Qt::Vertical);
        }
        QSignalSpy splitSpy(actions.value(QStringLiteral("cut_timeline_clip")), &QAction::triggered);
        auto *split = workspace.findChild<QToolButton *>(QStringLiteral("mobile_cut_timeline_clip"));
        QVERIFY(!split->isVisible());
        workspace.showEditTools();
        QVERIFY(split->isVisible());
        split->click();
        QCOMPARE(splitSpy.count(), 1);
        actions.value(QStringLiteral("cut_timeline_clip"))->setEnabled(false);
        QVERIFY(!split->isEnabled());
        split->click();
        QCOMPARE(splitSpy.count(), 1);
        workspace.findChild<QToolButton *>(QStringLiteral("mobile_tools"))->click();
        QVERIFY(!split->isVisible());
        auto *captions = workspace.findChild<QToolButton *>(QStringLiteral("mobile_captions"));
        QVERIFY(captions->width() >= captions->fontMetrics().horizontalAdvance(captions->text()) + 12);

        // A wide desktop panel scrolls inside the sheet without widening the phone.
        auto *panel = new QLabel(QStringLiteral("Effect settings"));
        panel->setMinimumSize(900, 600);
        workspace.showPanel(panel, QStringLiteral("Effects"));
        QTest::qWait(20);
        QCOMPARE(workspace.size(), QSize(430, 876));
        QVERIFY(preview->isVisible());
        QVERIFY(!timeline->isVisible());
        QVERIFY(panel->isVisible());
        QCOMPARE(workspace.takePanel(), panel);
        QVERIFY(timeline->isVisible());
        delete panel;

        QSignalSpy commandSpy(&workspace, &KdenCut::MobileWorkspace::command);
        workspace.findChild<QToolButton *>(QStringLiteral("mobile_media"))->click();
        QCOMPARE(commandSpy.last().at(0).toString(), QStringLiteral("media"));
        workspace.findChild<QToolButton *>(QStringLiteral("mobile_home"))->click();
        QVERIFY(workspace.findChild<QToolButton *>(QStringLiteral("mobileNew"))->isVisible());
        workspace.resize(430, 344);
        QTest::qWait(20);
        QCOMPARE(workspace.size(), QSize(430, 344));
        auto *resume = workspace.findChild<QToolButton *>(QStringLiteral("mobile_continue"));
        QScrollArea *homeScroll = nullptr;
        for (auto *scroll : workspace.findChildren<QScrollArea *>()) {
            if (scroll->isVisible() && scroll->isAncestorOf(resume)) {
                homeScroll = scroll;
                break;
            }
        }
        QVERIFY(homeScroll);
        homeScroll->ensureWidgetVisible(resume);
        QTest::qWait(20);
        QVERIFY(homeScroll->viewport()->rect().contains(QRect(resume->mapTo(homeScroll->viewport(), QPoint()), resume->size())));
        resume->click();
        QVERIFY(preview->isVisible());
        workspace.showEditor();
        QVERIFY(preview->isVisible());
    }
};

QTEST_MAIN(MobileChecks)
#include "check_mobile_workspace.moc"
