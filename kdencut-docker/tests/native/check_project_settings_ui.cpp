/* SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL */
#include "../../../src/kdencut/mobilesettings.h"
#include "ui_projectsettings_ui.h"
#include <QApplication>
#include <QComboBox>
#include <QFile>
#include <QPushButton>
#include <QScrollBar>
#include <QtTest>

class ProjectSettingsUiChecks : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void realFormFitsPhone()
    {
        QFile qss(QStringLiteral(KDENCUT_SOURCE_DIR "/src/kdencut/kdencut-touch.qss"));
        QVERIFY(qss.open(QIODevice::ReadOnly));
        qApp->setStyleSheet(QString::fromUtf8(qss.readAll()));
        QDialog dialog;
        Ui::ProjectSettings_UI ui;
        ui.setupUi(&dialog);
        QVERIFY(KdenCut::adaptProjectSettings(&dialog, QRect(0, 0, 320, 568)));
        dialog.show();
        QTest::qWait(50);
        auto *sections = dialog.findChild<QComboBox *>(QStringLiteral("kdencutSettingsSection"));
        QVERIFY(sections);
        for (int i = 0; i < sections->count(); ++i) {
            sections->setCurrentIndex(i);
            QTest::qWait(20);
            QVERIFY(dialog.width() <= 320);
            QVERIFY(dialog.height() <= 568);
            const auto areas = ui.tabWidget->currentWidget()->findChildren<QScrollArea *>();
            QVERIFY(!areas.isEmpty());
            for (auto *area : areas) {
                if (area->isVisible()) QCOMPARE(area->horizontalScrollBar()->maximum(), 0);
            }
            for (auto *button : ui.buttonBox->buttons()) {
                QVERIFY(dialog.rect().contains(QRect(button->mapTo(&dialog, QPoint()), button->size())));
            }
        }
        sections->setCurrentIndex(0);
        auto *folder = dialog.findChild<QComboBox *>(QStringLiteral("kdencutProjectFolder"));
        QVERIFY(folder);
        folder->setCurrentIndex(2);
        QVERIFY(ui.custom_folder->isChecked());
        QVERIFY(ui.project_folder->isEnabled());
        folder->setCurrentIndex(0);
        QVERIFY(ui.default_folder->isChecked());
        QVERIFY(!ui.project_folder->isEnabled());
        QSignalSpy accepted(&dialog, &QDialog::accepted);
        QTest::mouseClick(ui.buttonBox->button(QDialogButtonBox::Ok), Qt::LeftButton);
        QCOMPARE(accepted.count(), 1);
    }
};
QTEST_MAIN(ProjectSettingsUiChecks)
#include "check_project_settings_ui.moc"
