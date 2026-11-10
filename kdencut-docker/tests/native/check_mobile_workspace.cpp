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
#include <QVBoxLayout>
#include <QtTest>
#ifdef CHECK_KDDOCK
#include <kddockwidgets/DockWidget.h>
#include <kddockwidgets/MainWindow.h>
#include <kddockwidgets/LayoutSaver.h>
#include <kddockwidgets/Config.h>
#endif

class MobileChecks : public QObject
{
    Q_OBJECT
private Q_SLOTS:
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
        for (const QSize &size : {QSize(320, 568), QSize(375, 667), QSize(430, 876), QSize(932, 374), QSize(430, 344), QSize(430, 876)}) {
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
        split->click();
        QCOMPARE(splitSpy.count(), 1);
        actions.value(QStringLiteral("cut_timeline_clip"))->setEnabled(false);
        QVERIFY(!split->isEnabled());
        split->click();
        QCOMPARE(splitSpy.count(), 1);

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
