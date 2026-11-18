/* SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL */
#include "../../../src/kdencut/mobilechrome.h"
#include <QLabel>
#include <QTabBar>
#include <QVBoxLayout>
#include <QtTest>

class MobileChromeChecks : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void savedDesktopBarsStayHidden()
    {
        QMainWindow window;
        auto *preview = new QLabel(QStringLiteral("Video"));
        window.setCentralWidget(preview);
        auto *mainBar = window.addToolBar(QStringLiteral("Main"));
        mainBar->setObjectName(QStringLiteral("mainToolBar"));
        mainBar->addAction(QStringLiteral("Save"));
        auto *hiddenBar = window.addToolBar(QStringLiteral("Hidden"));
        hiddenBar->setObjectName(QStringLiteral("hiddenToolBar"));
        hiddenBar->hide();
        auto *menu = window.menuBar();
        menu->addMenu(QStringLiteral("File"));
        auto *tabs = new QTabBar;
        for (const QString &label : {QStringLiteral("Editing"), QStringLiteral("Audio"), QStringLiteral("Effects"), QStringLiteral("Color")}) tabs->addTab(label);
        menu->setCornerWidget(tabs);
        auto *status = window.statusBar();
        status->showMessage(QStringLiteral("Ready"));
        window.resize(430, 876);
        window.show();
        QTest::qWait(10);
        QVERIFY(mainBar->isVisible());
        QVERIFY(tabs->isVisible());
        QVERIFY(preview->mapTo(&window, QPoint()).y() > 0);
        const QByteArray desktop = window.saveState();

        {
            KdenCut::MobileChrome chrome(&window, preview);
            QTest::qWait(10);
            QVERIFY(mainBar->isHidden());
            QVERIFY(menu->isHidden());
            QVERIFY(status->isHidden());
            QVERIFY(!tabs->isVisible());
            QCOMPARE(preview->mapTo(&window, QPoint()).y(), 0);

            // Reproduce KXmlGuiWindow restoring the saved desktop after the
            // phone workspace has already hidden its toolbars.
            QVERIFY(window.restoreState(desktop));
            menu->show();
            status->show();
            hiddenBar->show();
            window.showFullScreen();
            window.resize(876, 430);
            QTest::qWait(10);
            QVERIFY(mainBar->isHidden());
            QVERIFY(hiddenBar->isHidden());
            QVERIFY(menu->isHidden());
            QVERIFY(status->isHidden());
            QCOMPARE(preview->mapTo(&window, QPoint()).y(), 0);
        }
        QTest::qWait(10);
        QVERIFY(mainBar->isVisible());
        QVERIFY(tabs->isVisible());
        QVERIFY(status->isVisible());
        QVERIFY(hiddenBar->isHidden());
    }

    void lateBarsAndOtherWindows()
    {
        QMainWindow window;
        auto *panel = new QWidget;
        auto *layout = new QVBoxLayout(panel);
        auto *panelTools = new QToolBar(panel);
        panelTools->addAction(QStringLiteral("Import"));
        layout->addWidget(panelTools);
        window.setCentralWidget(panel);
        QMainWindow otherWindow;
        auto *otherTools = otherWindow.addToolBar(QStringLiteral("Other editor"));
        otherTools->addAction(QStringLiteral("Save"));
        window.show();
        otherWindow.show();
        QToolBar *lateBar = nullptr;
        {
            KdenCut::MobileChrome chrome(&window, panel);
            lateBar = window.addToolBar(QStringLiteral("Recreated toolbar"));
            lateBar->addAction(QStringLiteral("Save"));
            lateBar->show();
            QTest::qWait(10);
            QVERIFY(lateBar->isHidden());
            QVERIFY(panelTools->isVisible());
            QVERIFY(otherTools->isVisible());
            auto *deletedBar = window.addToolBar(QStringLiteral("Removed toolbar"));
            deletedBar->show();
            delete deletedBar;
        }
        QVERIFY(lateBar->isVisible());
        QVERIFY(panelTools->isVisible());
        QVERIFY(otherTools->isVisible());
    }
};

QTEST_MAIN(MobileChromeChecks)
#include "check_mobile_chrome.moc"
