/* SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL */
#include <QApplication>
#include <QComboBox>
#include <QFile>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QtTest>
#include "../../../src/kdencut/palettewatcher.h"

class ThemeChecks : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void inputsFollowPalette()
    {
        QFile file(QStringLiteral(KDENCUT_SOURCE_DIR "/src/kdencut/kdencut-touch.qss"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QWidget window;
        KdenCut::watchStylePalette(&window);
        auto *layout = new QVBoxLayout(&window);
        auto *line = new QLineEdit(QStringLiteral("Phone project"));
        auto *spin = new QSpinBox;
        spin->setValue(30);
        auto *combo = new QComboBox;
        combo->addItem(QStringLiteral("1080p"));
        for (auto *input : {static_cast<QWidget *>(line), static_cast<QWidget *>(spin), static_cast<QWidget *>(combo)}) layout->addWidget(input);
        qApp->setStyleSheet(QString::fromUtf8(file.readAll()));
        auto *button = new QPushButton(QStringLiteral("Cancel"));
        layout->addWidget(button);
        window.resize(390, 320);
        window.show();
        for (bool light : {true, false, true}) {
            QPalette palette;
            palette.setColor(QPalette::Base, light ? QColor("#fafafa") : QColor("#121212"));
            palette.setColor(QPalette::Text, light ? QColor("#121212") : QColor("#fafafa"));
            palette.setColor(QPalette::Button, light ? QColor("#dddddd") : QColor("#26262b"));
            palette.setColor(QPalette::ButtonText, light ? QColor("#121212") : QColor("#fafafa"));
            palette.setColor(QPalette::Highlight, QColor("#00838c"));
            palette.setColor(QPalette::HighlightedText, Qt::white);
            qApp->setPalette(palette);
            QTest::qWait(10);
            QCOMPARE(button->palette().color(QPalette::ButtonText), palette.color(QPalette::ButtonText));
            QCOMPARE(button->grab().toImage().pixelColor(button->width() / 2, button->height() - 5), palette.color(QPalette::Button));
            for (auto *input : {static_cast<QWidget *>(line), static_cast<QWidget *>(spin), static_cast<QWidget *>(combo)}) {
                QCOMPARE(input->palette().color(QPalette::Base), palette.color(QPalette::Base));
                QCOMPARE(input->palette().color(QPalette::Text), palette.color(QPalette::Text));
                const auto image = input->grab().toImage();
                QCOMPARE(image.pixelColor(input->width() / 2, input->height() - 5), palette.color(QPalette::Base));
            }
        }
    }
};
QTEST_MAIN(ThemeChecks)
#include "check_touch_theme.moc"
