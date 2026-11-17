/* SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL */
#include "mobilesettings.h"
#include "touchscrolling.h"

#include <QAbstractButton>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPersistentModelIndex>
#include <QRadioButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTabBar>
#include <QTabWidget>
#include <QTextEdit>
#include <QTreeView>
#include <QVBoxLayout>
#include <algorithm>
#include <functional>

namespace {
void fitCombo(QComboBox *combo)
{
    combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    combo->setMinimumContentsLength(1);
    combo->setMinimumWidth(0);
    combo->setMinimumHeight(48);
    combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    combo->view()->setTextElideMode(Qt::ElideRight);
    KdenCut::enableSwipeScrolling(combo->view(), true);
}

void presetDropdown(QGroupBox *box)
{
    auto *tree = box->findChild<QTreeView *>();
    auto *splitter = tree ? qobject_cast<QSplitter *>(tree->parentWidget()) : nullptr;
    if (!splitter || !tree->model() || !tree->selectionModel()) return;
    auto *combo = new QComboBox(splitter);
    combo->setObjectName(QStringLiteral("kdencutProjectPreset"));
    combo->setAccessibleName(box->title());
    fitCombo(combo);
    splitter->insertWidget(splitter->indexOf(tree), combo);
    tree->hide();
    auto syncSelection = [combo, tree]() {
        const QSignalBlocker blocker(combo);
        int selected = -1;
        for (int i = 0; i < combo->count(); ++i) {
            if (combo->itemData(i).value<QPersistentModelIndex>() == tree->currentIndex()) selected = i;
        }
        combo->setCurrentIndex(selected);
        combo->setToolTip(combo->currentText());
    };
    auto rebuild = [combo, tree, syncSelection]() {
        const QSignalBlocker blocker(combo);
        combo->clear();
        // Keep persistent indexes into the existing filtered model: selection
        // still runs ProfileWidget's validation and updates its preview.
        std::function<void(const QModelIndex &, const QString &)> visit;
        visit = [&](const QModelIndex &parent, const QString &group) {
            auto *model = tree->model();
            for (int row = 0; row < model->rowCount(parent); ++row) {
                const QModelIndex index = model->index(row, 0, parent);
                const QString name = index.data().toString();
                if (model->hasChildren(index)) {
                    visit(index, name);
                } else if ((index.flags() & Qt::ItemIsSelectable) && (index.flags() & Qt::ItemIsEnabled)) {
                    combo->addItem(group.isEmpty() ? name : group + QStringLiteral(" / ") + name, QVariant::fromValue(QPersistentModelIndex(index)));
                }
            }
        };
        visit({}, {});
        syncSelection();
    };
    QObject::connect(combo, &QComboBox::currentIndexChanged, tree, [combo, tree](int row) {
        const auto index = combo->itemData(row).value<QPersistentModelIndex>();
        if (index.isValid()) tree->selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    });
    QObject::connect(tree->selectionModel(), &QItemSelectionModel::currentChanged, combo, syncSelection);
    auto *model = tree->model();
    QObject::connect(model, &QAbstractItemModel::modelReset, combo, rebuild);
    QObject::connect(model, &QAbstractItemModel::layoutChanged, combo, rebuild);
    QObject::connect(model, &QAbstractItemModel::rowsInserted, combo, rebuild);
    QObject::connect(model, &QAbstractItemModel::rowsRemoved, combo, rebuild);
    QObject::connect(model, &QAbstractItemModel::dataChanged, combo, rebuild);
    rebuild();
}

void folderDropdown(QDialog *dialog)
{
    auto *form = dialog->findChild<QFormLayout *>(QStringLiteral("formLayout"));
    const QStringList names{QStringLiteral("default_folder"), QStringLiteral("same_folder"), QStringLiteral("custom_folder")};
    QList<QRadioButton *> choices;
    for (const QString &name : names) {
        auto *choice = dialog->findChild<QRadioButton *>(name);
        if (!choice) return;
        choices << choice;
    }
    if (!form) return;
    auto *combo = new QComboBox;
    combo->setObjectName(QStringLiteral("kdencutProjectFolder"));
    fitCombo(combo);
    int row = 0;
    QFormLayout::ItemRole role;
    form->getWidgetPosition(choices.first(), &row, &role);
    if (row < 0) { delete combo; return; }
    auto *oldItem = form->replaceWidget(choices.first(), combo);
    delete oldItem;
    if (auto *labelItem = form->itemAt(row, QFormLayout::LabelRole)) {
        if (auto *label = qobject_cast<QLabel *>(labelItem->widget())) {
            label->setBuddy(combo);
            combo->setAccessibleName(label->text());
        }
    }
    for (int i = 0; i < choices.size(); ++i) {
        auto *choice = choices.at(i);
        combo->addItem(choice->text());
        combo->setItemData(i, choice->text(), Qt::ToolTipRole);
        if (choice->isChecked()) combo->setCurrentIndex(i);
        choice->hide();
        QObject::connect(choice, &QRadioButton::toggled, combo, [combo, i](bool checked) {
            if (checked) { const QSignalBlocker blocker(combo); combo->setCurrentIndex(i); }
        });
    }
    QObject::connect(combo, &QComboBox::currentIndexChanged, dialog, [choices](int index) {
        // click() preserves the original radio signals and custom-folder field.
        if (index >= 0 && index < choices.size()) choices.at(index)->click();
    });
}

void stackWidget(QWidget *widget);
void stackLayout(QLayout *layout)
{
    if (!layout) return;
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(8);
    if (auto *form = qobject_cast<QFormLayout *>(layout)) {
        form->setRowWrapPolicy(QFormLayout::WrapAllRows);
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        form->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
    } else if (auto *box = qobject_cast<QBoxLayout *>(layout)) {
        box->setDirection(QBoxLayout::TopToBottom);
    } else if (auto *grid = qobject_cast<QGridLayout *>(layout)) {
        struct Entry { QLayoutItem *item; int row; int column; };
        QList<Entry> entries;
        const int columns = grid->columnCount(), rows = grid->rowCount();
        while (grid->count()) {
            int row, column, rowSpan, columnSpan;
            grid->getItemPosition(0, &row, &column, &rowSpan, &columnSpan);
            entries << Entry{grid->takeAt(0), row, column};
        }
        for (int c = 0; c < columns; ++c) { grid->setColumnStretch(c, 0); grid->setColumnMinimumWidth(c, 0); }
        for (int r = 0; r < rows; ++r) { grid->setRowStretch(r, 0); grid->setRowMinimumHeight(r, 0); }
        std::stable_sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) {
            return a.row == b.row ? a.column < b.column : a.row < b.row;
        });
        for (int r = 0; r < entries.size(); ++r) grid->addItem(entries.at(r).item, r, 0);
    }
    for (int i = 0; i < layout->count(); ++i) {
        auto *item = layout->itemAt(i);
        if (item->widget()) stackWidget(item->widget());
        else if (item->layout()) stackLayout(item->layout());
        else if (item->spacerItem()) item->spacerItem()->changeSize(0, 0, QSizePolicy::Minimum, QSizePolicy::Minimum);
    }
}

void stackWidget(QWidget *widget)
{
    if (qobject_cast<QDialogButtonBox *>(widget)) return;
    widget->setMinimumWidth(0);
    if (auto *combo = qobject_cast<QComboBox *>(widget)) { fitCombo(combo); return; }
    if (auto *label = qobject_cast<QLabel *>(widget)) { label->setWordWrap(true); return; }
    if (auto *button = qobject_cast<QAbstractButton *>(widget)) {
        button->setMinimumHeight(48);
        // Qt buttons support multiline labels. Keep long translated choices
        // readable without forcing the page wider than the phone.
        const QString text = button->text();
        QString wrapped, line;
        for (const QString &word : text.split(QLatin1Char(' '))) {
            const QString next = line.isEmpty() ? word : line + QLatin1Char(' ') + word;
            if (!line.isEmpty() && button->fontMetrics().horizontalAdvance(next) > 170) {
                wrapped += line + QLatin1Char('\n'); line = word;
            } else line = next;
        }
        button->setText(wrapped + line);
        button->setAccessibleName(text);
        return;
    }
    // Composite input widgets keep their internal browse/clear/step buttons.
    if (qobject_cast<QLineEdit *>(widget) || widget->inherits("QAbstractSpinBox") || widget->inherits("KUrlRequester")) return;
    if (auto *scroll = qobject_cast<QScrollArea *>(widget)) {
        scroll->setWidgetResizable(true);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        if (scroll->widget()) stackWidget(scroll->widget());
        KdenCut::enableSwipeScrolling(scroll, true);
        return;
    }
    if (auto *view = qobject_cast<QAbstractItemView *>(widget)) {
        if (!view->dragEnabled()) {
            view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
            KdenCut::enableSwipeScrolling(view, true);
        }
        return;
    }
    if (auto *edit = qobject_cast<QTextEdit *>(widget)) {
        if (edit->isReadOnly()) KdenCut::enableSwipeScrolling(edit, true);
        return;
    }
    if (auto *splitter = qobject_cast<QSplitter *>(widget)) {
        splitter->setOrientation(Qt::Vertical);
        for (int i = 0; i < splitter->count(); ++i) stackWidget(splitter->widget(i));
        return;
    }
    stackLayout(widget->layout());
    if (auto *box = qobject_cast<QGroupBox *>(widget); box && !box->isCheckable() && !box->title().isEmpty()) {
        // A group-box title cannot wrap, but its replacement label can.
        auto *label = new QLabel(box->title());
        label->setWordWrap(true);
        box->setAccessibleName(box->title());
        box->setTitle(QString());
        auto *body = new QWidget;
        body->setLayout(box->layout());
        auto *layout = new QVBoxLayout(box);
        layout->setContentsMargins(4, 4, 4, 4);
        layout->addWidget(label);
        layout->addWidget(body);
    }
}
}

bool KdenCut::adaptProjectSettings(QDialog *dialog, const QRect &available)
{
    if (dialog->objectName() != QLatin1String("ProjectSettings_UI")) return false;
    auto *tabs = dialog->findChild<QTabWidget *>(QStringLiteral("tabWidget"));
    auto *buttons = dialog->findChild<QDialogButtonBox *>();
    if (!tabs || !buttons) return false;
    if (!dialog->property("kdencutVerticalSettings").toBool()) {
        dialog->setProperty("kdencutVerticalSettings", true);
        if (auto *profiles = dialog->findChild<QGroupBox *>(QStringLiteral("profile_box"))) presetDropdown(profiles);
        folderDropdown(dialog);
        for (int i = 0; i < tabs->count(); ++i) {
            QWidget *page = tabs->widget(i);
            stackWidget(page);
            if (!page->findChild<QScrollArea *>()) {
                auto *body = new QWidget;
                body->setLayout(page->layout());
                auto *scroll = new QScrollArea;
                scroll->setWidgetResizable(true);
                scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
                scroll->setWidget(body);
                enableSwipeScrolling(scroll, true);
                auto *layout = new QVBoxLayout(page);
                layout->setContentsMargins(0, 0, 0, 0);
                layout->addWidget(scroll);
            }
        }
        auto *sections = new QComboBox(dialog);
        sections->setObjectName(QStringLiteral("kdencutSettingsSection"));
        sections->setAccessibleName(dialog->windowTitle());
        fitCombo(sections);
        for (int i = 0; i < tabs->count(); ++i) {
            if (tabs->isTabVisible(i) && tabs->isTabEnabled(i)) sections->addItem(tabs->tabText(i), i);
        }
        sections->setCurrentIndex(sections->findData(tabs->currentIndex()));
        QObject::connect(sections, &QComboBox::currentIndexChanged, tabs, [sections, tabs](int i) { tabs->setCurrentIndex(sections->itemData(i).toInt()); });
        QObject::connect(tabs, &QTabWidget::currentChanged, sections, [sections](int i) {
            const QSignalBlocker blocker(sections); sections->setCurrentIndex(sections->findData(i));
        });
        tabs->tabBar()->hide();
        tabs->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
        // Keep accept/cancel outside the scrolling page, always within reach.
        delete dialog->layout();
        auto *layout = new QVBoxLayout(dialog);
        layout->setContentsMargins(6, 6, 6, 6);
        layout->setSizeConstraint(QLayout::SetNoConstraint);
        layout->addWidget(sections);
        layout->addWidget(tabs, 1);
        layout->addWidget(buttons);
        buttons->setOrientation(Qt::Vertical);
    }
    dialog->setMinimumSize(0, 0);
    dialog->resize(qMax(240, available.width() - 16), qMax(200, available.height() - 40));
    dialog->move(available.center() - QPoint(dialog->width() / 2, dialog->height() / 2));
    return true;
}
