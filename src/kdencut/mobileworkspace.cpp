/* SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL */
#include "mobileworkspace.h"

#include <QAction>
#include <QDialog>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScroller>
#include <QSplitter>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

using namespace KdenCut;

void MobileWorkspace::fitDialog(QDialog *dialog, const QRect &available)
{
    if (!dialog || !dialog->layout()) {
        return;
    }
    const QSize limit(qMax(240, available.width() - 24), qMax(200, available.height() - 48));
    const QSize wanted = dialog->sizeHint().expandedTo(dialog->minimumSizeHint()).expandedTo(dialog->minimumSize());
    if (dialog->property("kdencutMobileDialog").toBool()) {
        dialog->resize(wanted.boundedTo(limit));
        dialog->move(available.center() - QPoint(dialog->width() / 2, dialog->height() / 2));
        return;
    }
    if (wanted.width() <= limit.width() && wanted.height() <= limit.height()) {
        return;
    }
    dialog->setProperty("kdencutMobileDialog", true);
    auto *body = new QWidget;
    // QWidget::setLayout transfers the existing layout and its children. All
    // original signals, validators and dialog accept/reject actions stay intact.
    body->setLayout(dialog->layout());
    auto *scroll = new QScrollArea(dialog);
    scroll->setWidgetResizable(true);
    scroll->setWidget(body);
    auto *layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSizeConstraint(QLayout::SetNoConstraint);
    layout->addWidget(scroll);
    dialog->setMinimumSize(0, 0);
    dialog->resize(wanted.boundedTo(limit));
    dialog->move(available.center() - QPoint(dialog->width() / 2, dialog->height() / 2));
}

MobileWorkspace::MobileWorkspace(QWidget *preview, QWidget *timeline, const QMap<QString, QAction *> &actions, QWidget *parent)
    : QWidget(parent)
    , m_actions(actions)
    , m_timeline(timeline)
{
    setObjectName(QStringLiteral("mobileWorkspace"));
    QFont mobileFont = font();
    mobileFont.setPixelSize(14);
    setFont(mobileFont);
    // Scope these rules to this workspace. Desktop controls retain their theme.
    setStyleSheet(QStringLiteral(
        "#mobileWorkspace { background: #121212; color: #eeeeee; }"
        "#mobileWorkspace QLabel { color: #eeeeee; background: transparent; }"
        "#mobileWorkspace QToolButton { min-width: 40px; min-height: 40px; padding: 3px 6px; border: 0; border-radius: 9px; background: transparent; color: #eeeeee; }"
        "#mobileWorkspace QToolButton:pressed { background: #28474a; }"
        "#mobileWorkspace QToolButton:disabled { color: #696969; }"
        "#mobileWorkspace QToolButton#mobileExport, #mobileWorkspace QToolButton#mobileNew { background: #08bfd0; color: #071719; font-weight: bold; }"
        "#mobileWorkspace QToolButton#mobileNew { min-height: 90px; font-size: 22px; }"
        "#mobileWorkspace QScrollArea { border: 0; background: #1d1d1d; }"
        "#mobileWorkspace QSplitter::handle { background: #333333; }"));
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    m_pages = new QStackedWidget(this);
    // Hidden pages must never impose their desktop minimum on the viewport.
    m_pages->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    root->addWidget(m_pages);

    m_editor = new QWidget;
    auto *editor = new QVBoxLayout(m_editor);
    editor->setContentsMargins(0, 0, 0, 0);
    editor->setSpacing(0);
    auto *header = new QHBoxLayout;
    header->setContentsMargins(8, 4, 8, 4);
    auto *home = button(tr("Home"), QStringLiteral("go-home"), QStringLiteral("home"), false);
    header->addWidget(home);
    auto *title = new QLabel(QStringLiteral("KdenCut"));
    title->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    header->addWidget(title, 1);
    header->addWidget(actionButton(QStringLiteral("file_save"), tr("Save"), QStringLiteral("document-save"), false));
    auto *exportButton = actionButton(QStringLiteral("project_render"), tr("Export"), QStringLiteral("document-export"), false);
    exportButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    exportButton->setObjectName(QStringLiteral("mobileExport"));
    header->addWidget(exportButton);
    editor->addLayout(header);

    m_splitter = new QSplitter(Qt::Vertical);
    m_splitter->setObjectName(QStringLiteral("mobileSplitter"));
    m_splitter->setChildrenCollapsible(false);
    m_splitter->setHandleWidth(6);
    auto *monitor = new QWidget;
    auto *monitorLayout = new QVBoxLayout(monitor);
    monitorLayout->setContentsMargins(0, 0, 0, 0);
    monitorLayout->setSpacing(0);
    preview->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    monitorLayout->addWidget(preview, 1);
    auto *transport = new QHBoxLayout;
    transport->setContentsMargins(8, 0, 8, 0);
    m_timecode = new QLabel(QStringLiteral("00:00:00:00"));
    m_timecode->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    transport->addWidget(m_timecode, 1);
    m_play = button(tr("Play"), QStringLiteral("media-playback-start"), QStringLiteral("play"), false);
    transport->addWidget(m_play);
    transport->addStretch();
    transport->addWidget(actionButton(QStringLiteral("edit_undo"), tr("Undo"), QStringLiteral("edit-undo"), false));
    transport->addWidget(actionButton(QStringLiteral("edit_redo"), tr("Redo"), QStringLiteral("edit-redo"), false));
    monitorLayout->addLayout(transport);
    m_splitter->addWidget(monitor);

    m_lower = new QStackedWidget;
    m_lower->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    m_timeline->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    auto *timelinePage = new QWidget;
    auto *timelineLayout = new QVBoxLayout(timelinePage);
    timelineLayout->setContentsMargins(0, 0, 0, 0);
    timelineLayout->setSpacing(0);
    timelineLayout->addWidget(toolStrip({
        button(tr("Add media"), QStringLiteral("list-add"), QStringLiteral("import"), false),
        actionButton(QStringLiteral("cut_timeline_clip"), tr("Split"), QStringLiteral("edit-cut"), false),
        actionButton(QStringLiteral("delete_timeline_selection"), tr("Delete"), QStringLiteral("edit-delete"), false),
        actionButton(QStringLiteral("duplicate_timeline_clip"), tr("Duplicate"), QStringLiteral("edit-copy"), false),
        actionButton(QStringLiteral("zoom_fit"), tr("Fit timeline"), QStringLiteral("zoom-fit-best"), false),
        actionButton(QStringLiteral("view_zoom_out"), tr("Zoom out"), QStringLiteral("zoom-out"), false),
        actionButton(QStringLiteral("view_zoom_in"), tr("Zoom in"), QStringLiteral("zoom-in"), false)}));
    timelineLayout->addWidget(m_timeline, 1);
    m_timeline = timelinePage;
    m_lower->addWidget(m_timeline);
    m_panelPage = new QWidget;
    auto *panelLayout = new QVBoxLayout(m_panelPage);
    panelLayout->setContentsMargins(0, 0, 0, 0);
    panelLayout->setSpacing(0);
    auto *panelHeader = new QHBoxLayout;
    panelHeader->setContentsMargins(12, 0, 8, 0);
    m_panelTitle = new QLabel;
    panelHeader->addWidget(m_panelTitle, 1);
    auto *done = button(tr("Done"), QStringLiteral("dialog-ok"), QStringLiteral("done"), false);
    done->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    panelHeader->addWidget(done);
    panelLayout->addLayout(panelHeader);
    m_mediaTools = toolStrip({button(tr("Import"), QStringLiteral("list-add"), QStringLiteral("import")),
                             button(tr("Add to timeline"), QStringLiteral("insert-object"), QStringLiteral("insert"))});
    panelLayout->addWidget(m_mediaTools);
    m_panelScroll = new QScrollArea;
    m_panelScroll->setWidgetResizable(true);
    panelLayout->addWidget(m_panelScroll, 1);
    m_lower->addWidget(m_panelPage);
    m_splitter->addWidget(m_lower);
    m_splitter->setStretchFactor(0, 3);
    m_splitter->setStretchFactor(1, 2);
    m_splitter->setSizes({600, 360});
    editor->addWidget(m_splitter, 1);
    editor->addWidget(toolStrip({
        button(tr("Edit"), QStringLiteral("edit-cut"), QStringLiteral("edit")),
        button(tr("Media"), QStringLiteral("folder-videos"), QStringLiteral("media")),
        button(tr("Audio"), QStringLiteral("audio-x-generic"), QStringLiteral("audio")),
        button(tr("Text"), QStringLiteral("draw-text"), QStringLiteral("text")),
        button(tr("Effects"), QStringLiteral("tools-wizard"), QStringLiteral("effects")),
        button(tr("Adjust"), QStringLiteral("configure"), QStringLiteral("adjust")),
        button(tr("Captions"), QStringLiteral("add-subtitle"), QStringLiteral("captions")),
        button(tr("Transitions"), QStringLiteral("view-split-left-right"), QStringLiteral("transitions"))}));
    m_pages->addWidget(m_editor);

    auto *homePage = new QWidget;
    auto *homeLayout = new QVBoxLayout(homePage);
    homeLayout->setContentsMargins(20, 20, 20, 20);
    auto *brand = new QLabel(QStringLiteral("KdenCut"));
    QFont brandFont = font();
    brandFont.setPointSize(28);
    brandFont.setBold(true);
    brand->setFont(brandFont);
    homeLayout->addWidget(brand);
    homeLayout->addWidget(new QLabel(tr("Your next video starts here.")));
    homeLayout->addSpacing(24);
    auto *newVideo = button(tr("New video"), QStringLiteral("list-add"), QStringLiteral("new"));
    newVideo->setObjectName(QStringLiteral("mobileNew"));
    homeLayout->addWidget(newVideo);
    homeLayout->addWidget(actionButton(QStringLiteral("file_open"), tr("Open project"), QStringLiteral("document-open")));
    homeLayout->addWidget(button(tr("Continue editing"), QStringLiteral("go-next"), QStringLiteral("continue")));
    homeLayout->addStretch();
    homeLayout->addWidget(actionButton(QStringLiteral("kdencut_touch_mode"), tr("Desktop controls"), QStringLiteral("video-display")));
    auto *homeScroll = new QScrollArea;
    homeScroll->setWidgetResizable(true);
    homeScroll->setWidget(homePage);
    m_pages->addWidget(homeScroll);
    connect(this, &MobileWorkspace::command, this, [this, homeScroll](const QString &name) {
        if (name == QLatin1String("home")) {
            Q_EMIT panelClosed();
            m_pages->setCurrentWidget(homeScroll);
        } else if (name == QLatin1String("continue")) {
            showEditor();
        } else if (name == QLatin1String("done")) {
            Q_EMIT panelClosed();
        }
    });
}

QToolButton *MobileWorkspace::button(const QString &text, const QString &icon, const QString &name, bool label)
{
    auto *b = new QToolButton(this);
    b->setObjectName(QStringLiteral("mobile_") + name);
    b->setText(text);
    b->setToolTip(text);
    b->setAccessibleName(text);
    b->setIcon(QIcon::fromTheme(icon));
    b->setIconSize(QSize(24, 24));
    b->setToolButtonStyle(label ? Qt::ToolButtonTextUnderIcon : Qt::ToolButtonIconOnly);
    b->setFocusPolicy(Qt::NoFocus);
    connect(b, &QToolButton::clicked, this, [this, name]() { Q_EMIT command(name); });
    return b;
}

QToolButton *MobileWorkspace::actionButton(const QString &name, const QString &text, const QString &icon, bool label)
{
    auto *b = button(text, icon, name, label);
    QAction *action = m_actions.value(name);
    b->setEnabled(action && action->isEnabled());
    if (action) {
        connect(action, &QAction::changed, b, [action, b]() { b->setEnabled(action->isEnabled()); });
        connect(b, &QToolButton::clicked, action, &QAction::trigger);
    }
    return b;
}

QWidget *MobileWorkspace::toolStrip(const QList<QWidget *> &buttons)
{
    auto *scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("mobileToolStrip"));
    // Selkies sends mouse drags; do not replace this with a native TouchGesture.
    scroll->setProperty("kdencutScroller", true);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *contents = new QWidget;
    auto *row = new QHBoxLayout(contents);
    row->setContentsMargins(8, 4, 8, 4);
    row->setSpacing(6);
    for (QWidget *b : buttons) {
        row->addWidget(b);
    }
    row->addStretch();
    scroll->setWidget(contents);
    scroll->setFixedHeight(contents->sizeHint().height() + 2);
    QScroller::grabGesture(scroll->viewport(), QScroller::LeftMouseButtonGesture);
    return scroll;
}

void MobileWorkspace::showPanel(QWidget *panel, const QString &title, bool media)
{
    m_panelTitle->setText(title);
    m_mediaTools->setVisible(media);
    m_panelScroll->setWidget(panel);
    m_lower->setCurrentWidget(m_panelPage);
    showEditor();
}

QWidget *MobileWorkspace::takePanel()
{
    QWidget *panel = m_panelScroll->takeWidget();
    m_lower->setCurrentWidget(m_timeline);
    return panel;
}

void MobileWorkspace::showEditor() { m_pages->setCurrentWidget(m_editor); }
void MobileWorkspace::setPlaying(bool playing)
{
    m_play->setIcon(QIcon::fromTheme(playing ? QStringLiteral("media-playback-pause") : QStringLiteral("media-playback-start")));
    m_play->setAccessibleName(playing ? tr("Pause") : tr("Play"));
    m_play->setToolTip(m_play->accessibleName());
}
void MobileWorkspace::setTimecode(const QString &timecode) { m_timecode->setText(timecode); }
QSize MobileWorkspace::sizeHint() const { return QSize(430, 820); }
QSize MobileWorkspace::minimumSizeHint() const { return QSize(280, 280); }

void MobileWorkspace::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    const bool landscape = width() > height();
    if (landscape != m_landscape) {
        m_landscape = landscape;
        m_splitter->setOrientation(landscape ? Qt::Horizontal : Qt::Vertical);
        m_splitter->setSizes({600, 360});
    }
}
