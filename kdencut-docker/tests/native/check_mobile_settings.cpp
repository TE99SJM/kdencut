/* SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL */
#include "../../../src/kdencut/mobilesettings.h"
#include "../../../src/kdencut/touchscrolling.h"
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSortFilterProxyModel>
#include <QSpinBox>
#include <QSplitter>
#include <QStandardItemModel>
#include <QStyleOptionSpinBox>
#include <QTabBar>
#include <QTabWidget>
#include <QTreeView>
#include <QVBoxLayout>
#include <QtTest>

class SettingsChecks : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase()
    {
        QFile qss(QStringLiteral(KDENCUT_SOURCE_DIR "/src/kdencut/kdencut-touch.qss"));
        QVERIFY(qss.open(QIODevice::ReadOnly));
        qApp->setStyleSheet(QString::fromUtf8(qss.readAll()));
    }
    void portraitSettings_data()
    {
        QTest::addColumn<QSize>("screen");
        QTest::newRow("small phone") << QSize(320, 568);
        QTest::newRow("phone") << QSize(430, 932);
        QTest::newRow("tablet") << QSize(768, 1024);
        QTest::newRow("landscape tablet") << QSize(1366, 1024);
    }
    void portraitSettings()
    {
        QFETCH(QSize, screen);
        QDialog dialog;
        dialog.setObjectName(QStringLiteral("ProjectSettings_UI"));
        auto *root = new QVBoxLayout(&dialog);
        auto *tabs = new QTabWidget;
        tabs->setObjectName(QStringLiteral("tabWidget"));
        root->addWidget(tabs);
        auto *page = new QWidget;
        auto *pageLayout = new QVBoxLayout(page);
        auto *area = new QScrollArea;
        pageLayout->addWidget(area);
        auto *body = new QWidget;
        area->setWidget(body);
        auto *form = new QFormLayout(body);
        form->setObjectName(QStringLiteral("formLayout"));
        auto *defaultFolder = new QRadioButton(QStringLiteral("Default: /config/a/very/long/path/to/cache"));
        defaultFolder->setObjectName(QStringLiteral("default_folder"));
        auto *same = new QRadioButton(QStringLiteral("Same folder as the project"));
        same->setObjectName(QStringLiteral("same_folder"));
        auto *custom = new QRadioButton(QStringLiteral("Custom"));
        custom->setObjectName(QStringLiteral("custom_folder"));
        auto *path = new QLineEdit(QStringLiteral("/media/project"));
        path->setEnabled(false);
        form->addRow(QStringLiteral("Project folder"), defaultFolder);
        form->addRow(same);
        auto *customRow = new QHBoxLayout;
        customRow->addWidget(custom); customRow->addWidget(path);
        form->addRow(customRow);
        defaultFolder->setChecked(true);
        connect(custom, &QRadioButton::toggled, path, &QWidget::setEnabled);
        auto *profiles = new QGroupBox(QStringLiteral("Select the profile (preset) of the project"));
        profiles->setObjectName(QStringLiteral("profile_box"));
        auto *profileLayout = new QVBoxLayout(profiles);
        auto *splitter = new QSplitter;
        profileLayout->addWidget(splitter);
        auto *tree = new QTreeView;
        QStandardItemModel model;
        auto *category = new QStandardItem(QStringLiteral("Social"));
        category->setSelectable(false);
        category->appendRow(new QStandardItem(QStringLiteral("Portrait 1080p")));
        category->appendRow(new QStandardItem(QStringLiteral("Square 1080p")));
        model.appendRow(category);
        QSortFilterProxyModel proxy;
        proxy.setRecursiveFilteringEnabled(true);
        proxy.setSourceModel(&model);
        tree->setModel(&proxy);
        const auto portrait = proxy.index(0, 0, proxy.index(0, 0));
        tree->setCurrentIndex(portrait);
        splitter->addWidget(tree);
        splitter->addWidget(new QLabel(QStringLiteral("Profile preview")));
        form->addRow(profiles);
        for (int i = 0; i < 8; ++i) form->addRow(QStringLiteral("Setting %1").arg(i), new QSpinBox);
        tabs->addTab(page, QStringLiteral("Settings"));
        auto *proxyPage = new QWidget;
        auto *proxyLayout = new QHBoxLayout(proxyPage);
        proxyLayout->addWidget(new QLabel(QStringLiteral("Proxy resolution")));
        proxyLayout->addWidget(new QSpinBox);
        tabs->addTab(proxyPage, QStringLiteral("Proxy"));
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        root->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        QVERIFY(KdenCut::adaptProjectSettings(&dialog, QRect(QPoint(), screen)));
        dialog.show();
        QTest::qWait(30);
        QVERIFY(dialog.width() <= screen.width());
        QVERIFY(dialog.height() <= screen.height());
        QVERIFY(tabs->tabBar()->isHidden());
        QCOMPARE(area->horizontalScrollBar()->maximum(), 0);
        QVERIFY(area->verticalScrollBar()->maximum() > 0);
        auto *section = dialog.findChild<QComboBox *>(QStringLiteral("kdencutSettingsSection"));
        QVERIFY(section);
        section->setCurrentIndex(1);
        QCOMPARE(tabs->currentWidget(), proxyPage);
        tabs->setCurrentIndex(0);
        QCOMPARE(section->currentIndex(), 0);
        auto *folder = dialog.findChild<QComboBox *>(QStringLiteral("kdencutProjectFolder"));
        QVERIFY(folder);
        folder->setCurrentIndex(2);
        QVERIFY(custom->isChecked());
        QVERIFY(path->isEnabled());
        same->click();
        QCOMPARE(folder->currentIndex(), 1);
        QVERIFY(!path->isEnabled());
        QCOMPARE(path->text(), QStringLiteral("/media/project"));
        auto *preset = dialog.findChild<QComboBox *>(QStringLiteral("kdencutProjectPreset"));
        QVERIFY(preset);
        QCOMPARE(preset->count(), 2);
        QCOMPARE(preset->currentIndex(), 0);
        preset->setCurrentIndex(1);
        QCOMPARE(tree->currentIndex().data().toString(), QStringLiteral("Square 1080p"));
        proxy.setFilterFixedString(QStringLiteral("Portrait"));
        QCOMPARE(preset->count(), 1);
        QCOMPARE(preset->currentData().value<QPersistentModelIndex>(), QPersistentModelIndex(tree->currentIndex()));
        proxy.setFilterFixedString(QString());
        QCOMPARE(preset->count(), 2);
        tree->setCurrentIndex(portrait);
        QCOMPARE(preset->currentIndex(), 0);
        QVERIFY(KdenCut::adaptProjectSettings(&dialog, QRect(QPoint(), screen)));
        QCOMPARE(dialog.findChildren<QComboBox *>(QStringLiteral("kdencutSettingsSection")).size(), 1);
        for (auto *button : buttons->buttons()) {
            QVERIFY(button->width() >= 48);
            QVERIFY(button->height() >= 48);
            QVERIFY(dialog.rect().contains(QRect(button->mapTo(&dialog, QPoint()), button->size())));
        }
        // Keep the actions reachable through rotation and keyboard resizing.
        for (const QSize size : {QSize(932, 430), QSize(430, 344), screen}) {
            QVERIFY(KdenCut::adaptProjectSettings(&dialog, QRect(QPoint(), size)));
            QTest::qWait(20);
            QVERIFY(dialog.width() <= size.width() && dialog.height() <= size.height());
            for (auto *button : buttons->buttons()) {
                QVERIFY(dialog.rect().contains(QRect(button->mapTo(&dialog, QPoint()), button->size())));
            }
        }
        QSignalSpy rejected(&dialog, &QDialog::rejected);
        QTest::mouseClick(buttons->button(QDialogButtonBox::Cancel), Qt::LeftButton);
        QCOMPARE(rejected.count(), 1);
    }
    void dragScrollAndTap()
    {
        QScrollArea area;
        auto *body = new QWidget;
        auto *layout = new QVBoxLayout(body);
        auto *button = new QPushButton(QStringLiteral("Tap me"));
        layout->addWidget(button);
        for (int i = 0; i < 20; ++i) layout->addWidget(new QPushButton(QString::number(i)));
        area.setWidget(body);
        area.setWidgetResizable(true);
        area.resize(300, 400);
        KdenCut::enableSwipeScrolling(&area, true);
        area.show();
        QTest::qWait(30);
        QSignalSpy clicked(button, &QPushButton::clicked);
        QTest::mouseClick(button, Qt::LeftButton);
        QTRY_COMPARE(clicked.count(), 1);
        auto *viewport = area.viewport();
        QTest::mousePress(viewport, Qt::LeftButton, Qt::NoModifier, QPoint(150, 330));
        for (int y = 310; y >= 90; y -= 20) {
            const QPoint point(150, y);
            QMouseEvent move(QEvent::MouseMove, point, viewport->mapToGlobal(point), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
            QCoreApplication::sendEvent(viewport, &move);
            QTest::qWait(16);
        }
        QTest::mouseRelease(viewport, Qt::LeftButton, Qt::NoModifier, QPoint(150, 90));
        QTRY_VERIFY(area.verticalScrollBar()->value() > 100);
        const int released = area.verticalScrollBar()->value();
        QTest::qWait(100);
        QVERIFY(area.verticalScrollBar()->value() >= released);
        QCOMPARE(clicked.count(), 1);
        QScroller::scroller(viewport)->stop();
        area.verticalScrollBar()->setValue(0);
        QTest::qWait(30);
        const QPoint start = button->rect().center();
        QTest::mousePress(button, Qt::LeftButton, Qt::NoModifier, start);
        for (int delta = 20; delta <= 220; delta += 20) {
            const QPoint point = start - QPoint(0, delta);
            QMouseEvent move(QEvent::MouseMove, point, button->mapToGlobal(point), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
            QCoreApplication::sendEvent(button, &move);
            QTest::qWait(16);
        }
        QTest::mouseRelease(button, Qt::LeftButton, Qt::NoModifier, start - QPoint(0, 220));
        QTRY_VERIFY(area.verticalScrollBar()->value() > 50);
        QCOMPARE(clicked.count(), 1); // a swipe beginning on a button must not activate it
    }
    void dropdownOptionsAcceptFingerSizedTaps()
    {
        QComboBox combo;
        for (int i = 0; i < 30; ++i) combo.addItem(QStringLiteral("Preset %1").arg(i));
        combo.resize(280, 52);
        combo.show();
        combo.showPopup();
        QTest::qWait(30);
        auto *view = combo.view();
        KdenCut::enableSwipeScrolling(view, true);
        const QRect row = view->visualRect(combo.model()->index(1, 0));
        QVERIFY(row.height() >= 48);
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, row.center());
        QTRY_COMPARE(combo.currentIndex(), 1);
    }
    void numericButtonsHaveSeparateTouchTargets()
    {
        QSpinBox spin;
        spin.resize(280, 52);
        spin.show();
        QTest::qWait(10);
        QStyleOptionSpinBox option;
        option.initFrom(&spin);
        option.buttonSymbols = QAbstractSpinBox::UpDownArrows;
        const QRect up = spin.style()->subControlRect(QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxUp, &spin);
        const QRect down = spin.style()->subControlRect(QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxDown, &spin);
        QVERIFY(up.width() >= 44 && up.height() >= 44);
        QVERIFY(down.width() >= 44 && down.height() >= 44);
        QVERIFY2(!up.intersects(down), qPrintable(QStringLiteral("up %1,%2 down %3,%4").arg(up.x()).arg(up.width()).arg(down.x()).arg(down.width())));
        const auto hasSymbol = [](const QImage &image) {
            int pixels = 0;
            for (int y = 0; y < image.height(); ++y) for (int x = 0; x < image.width(); ++x) {
                const QColor color = image.pixelColor(x, y);
                if (qAbs(color.red() - 128) < 8 && qAbs(color.green() - 128) < 8 && qAbs(color.blue() - 128) < 8) ++pixels;
            }
            return pixels > 4;
        };
        const QImage rendered = spin.grab().toImage();
        QVERIFY(hasSymbol(rendered.copy(up)));
        QVERIFY(hasSymbol(rendered.copy(down)));
        QComboBox combo;
        combo.addItem(QStringLiteral("Project preset"));
        combo.resize(280, 52);
        combo.show();
        QTest::qWait(10);
        QStyleOptionComboBox comboOption;
        comboOption.initFrom(&combo);
        const QRect arrow = combo.style()->subControlRect(QStyle::CC_ComboBox, &comboOption, QStyle::SC_ComboBoxArrow, &combo);
        QVERIFY(arrow.width() >= 44);
        QVERIFY(hasSymbol(combo.grab().toImage().copy(arrow)));
        QTest::mouseClick(&spin, Qt::LeftButton, Qt::NoModifier, up.center());
        QCOMPARE(spin.value(), 1);
        QTest::mouseClick(&spin, Qt::LeftButton, Qt::NoModifier, down.center());
        QCOMPARE(spin.value(), 0);
    }
};
QTEST_MAIN(SettingsChecks)
#include "check_mobile_settings.moc"
