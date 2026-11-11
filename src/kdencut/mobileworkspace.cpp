/* SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL */
#include "mobileworkspace.h"

#include <QAction>
#include <QDialog>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScroller>
#include <QScrollerProperties>
#include <QSplitter>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

using namespace KdenCut;

namespace {
QIcon touchIcon(const QString &name)
{
    QPixmap image(48, 48);
    image.setDevicePixelRatio(2);
    image.fill(Qt::transparent);
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor("#e7e7e7"), 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    if (name == QLatin1String("go-home")) {
        p.drawLine(QPointF(5, 5), QPointF(19, 19)); p.drawLine(QPointF(19, 5), QPointF(5, 19));
    } else if (name == QLatin1String("media-playback-start")) {
        p.drawPolygon(QPolygonF{QPointF(8, 4), QPointF(19, 12), QPointF(8, 20)});
    } else if (name == QLatin1String("media-playback-pause")) {
        p.drawLine(QPointF(8, 5), QPointF(8, 19)); p.drawLine(QPointF(16, 5), QPointF(16, 19));
    } else if (name == QLatin1String("list-add")) {
        p.drawLine(QPointF(4, 12), QPointF(20, 12)); p.drawLine(QPointF(12, 4), QPointF(12, 20));
    } else if (name == QLatin1String("draw-text")) {
        p.drawLine(QPointF(4, 4), QPointF(20, 4)); p.drawLine(QPointF(12, 4), QPointF(12, 21));
    } else if (name == QLatin1String("audio-x-generic")) {
        p.drawEllipse(QRectF(4, 14, 7, 6)); p.drawLine(QPointF(11, 17), QPointF(11, 3));
        p.drawLine(QPointF(11, 3), QPointF(19, 6));
    } else if (name == QLatin1String("edit-cut")) {
        p.drawEllipse(QRectF(3, 15, 6, 6)); p.drawEllipse(QRectF(15, 15, 6, 6));
        p.drawLine(QPointF(8, 16), QPointF(19, 3)); p.drawLine(QPointF(16, 16), QPointF(5, 3));
    } else if (name == QLatin1String("tools-wizard")) {
        p.drawPolygon(QPolygonF{QPointF(12, 2), QPointF(15, 8), QPointF(22, 9), QPointF(17, 14), QPointF(18, 21), QPointF(12, 18), QPointF(6, 21), QPointF(7, 14), QPointF(2, 9), QPointF(9, 8)});
    } else if (name == QLatin1String("overlay")) {
        p.drawRect(QRectF(3, 3, 16, 16)); p.drawRect(QRectF(7, 7, 15, 15));
    } else if (name == QLatin1String("add-subtitle")) {
        p.drawRect(QRectF(3, 5, 18, 14)); p.drawLine(QPointF(6, 13), QPointF(18, 13)); p.drawLine(QPointF(6, 16), QPointF(14, 16));
    } else if (name == QLatin1String("filters")) {
        p.drawEllipse(QRectF(8, 2, 9, 10)); p.drawEllipse(QRectF(3, 11, 9, 10)); p.drawEllipse(QRectF(13, 11, 9, 10));
    } else if (name == QLatin1String("configure")) {
        for (int x : {5, 12, 19}) p.drawLine(QPointF(x, 3), QPointF(x, 21));
        p.drawLine(QPointF(2, 8), QPointF(8, 8)); p.drawLine(QPointF(9, 16), QPointF(15, 16)); p.drawLine(QPointF(16, 10), QPointF(22, 10));
    } else if (name == QLatin1String("view-split-left-right")) {
        p.drawRect(QRectF(2, 5, 20, 14)); p.drawLine(QPointF(8, 5), QPointF(16, 19)); p.drawLine(QPointF(16, 5), QPointF(8, 19));
    } else if (name == QLatin1String("edit-copy")) {
        p.drawRect(QRectF(8, 8, 13, 13)); p.drawPolyline(QPolygonF{QPointF(5, 16), QPointF(3, 16), QPointF(3, 3), QPointF(16, 3), QPointF(16, 5)});
    } else if (name == QLatin1String("edit-delete")) {
        p.drawLine(QPointF(3, 5), QPointF(21, 5)); p.drawRect(QRectF(9, 2, 6, 3)); p.drawRect(QRectF(6, 5, 12, 16));
    } else if (name == QLatin1String("zoom-in") || name == QLatin1String("zoom-out")) {
        p.drawEllipse(QRectF(2, 2, 14, 14)); p.drawLine(QPointF(15, 15), QPointF(22, 22)); p.drawLine(QPointF(5, 9), QPointF(13, 9));
        if (name == QLatin1String("zoom-in")) p.drawLine(QPointF(9, 5), QPointF(9, 13));
    } else if (name == QLatin1String("zoom-fit-best")) {
        p.drawRect(QRectF(3, 5, 18, 14)); p.drawLine(QPointF(7, 9), QPointF(17, 9)); p.drawLine(QPointF(7, 15), QPointF(17, 15));
    } else if (name == QLatin1String("edit-undo") || name == QLatin1String("edit-redo")) {
        if (name == QLatin1String("edit-redo")) { p.translate(24, 0); p.scale(-1, 1); }
        QPainterPath path; path.moveTo(5, 8); path.lineTo(15, 8); path.cubicTo(24, 8, 24, 20, 13, 20); p.drawPath(path);
        p.drawPolyline(QPolygonF{QPointF(9, 3), QPointF(4, 8), QPointF(9, 12)});
    } else if (name == QLatin1String("go-previous")) {
        p.drawPolyline(QPolygonF{QPointF(15, 3), QPointF(6, 12), QPointF(15, 21)});
    } else if (name == QLatin1String("view-fullscreen")) {
        p.drawPolyline(QPolygonF{QPointF(3, 9), QPointF(3, 3), QPointF(9, 3)});
        p.drawPolyline(QPolygonF{QPointF(15, 3), QPointF(21, 3), QPointF(21, 9)});
        p.drawPolyline(QPolygonF{QPointF(21, 15), QPointF(21, 21), QPointF(15, 21)});
        p.drawPolyline(QPolygonF{QPointF(9, 21), QPointF(3, 21), QPointF(3, 15)});
    } else {
        return QIcon::fromTheme(name);
    }
    return QIcon(image);
}

void enableSwipe(QScrollArea *area)
{
    area->setProperty("kdencutScroller", true);
    QScroller::grabGesture(area->viewport(), QScroller::LeftMouseButtonGesture);
    auto *scroller = QScroller::scroller(area->viewport());
    auto properties = scroller->scrollerProperties();
    properties.setScrollMetric(QScrollerProperties::FrameRate, QScrollerProperties::Fps60);
    properties.setScrollMetric(QScrollerProperties::DecelerationFactor, 0.12);
    properties.setScrollMetric(QScrollerProperties::HorizontalOvershootPolicy, QScrollerProperties::OvershootAlwaysOff);
    properties.setScrollMetric(QScrollerProperties::VerticalOvershootPolicy, QScrollerProperties::OvershootAlwaysOff);
    scroller->setScrollerProperties(properties);
}
} // namespace

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
    enableSwipe(scroll);
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
    QPalette dark = palette();
    dark.setColor(QPalette::Window, QColor("#121212"));
    dark.setColor(QPalette::Base, QColor("#1c1c1c"));
    dark.setColor(QPalette::WindowText, QColor("#eeeeee"));
    dark.setColor(QPalette::Text, QColor("#eeeeee"));
    setPalette(dark);
    // Scope these rules to this workspace. Desktop controls retain their theme.
    setStyleSheet(QStringLiteral(
        "#mobileWorkspace { background: #121212; color: #eeeeee; }"
        "#mobileWorkspace QLabel { color: #eeeeee; background: transparent; }"
        "#mobileWorkspace QToolButton { min-width: 40px; min-height: 40px; padding: 2px 4px; border: 0; border-radius: 7px; background: transparent; color: #eeeeee; }"
        "#mobileWorkspace QToolButton:pressed { background: #28474a; }"
        "#mobileWorkspace QToolButton:disabled { color: #696969; }"
        "#mobileWorkspace QToolButton#mobileExport, #mobileWorkspace QToolButton#mobileNew { background: #08bfd0; color: #071719; font-weight: bold; }"
        "#mobileWorkspace QToolButton#mobileNew { min-height: 90px; font-size: 22px; }"
        "#mobileWorkspace QScrollArea { border: 0; background: #1d1d1d; }"
        "#mobileWorkspace QWidget#mobileToolContents { background: #1d1d1d; }"
        "#mobileWorkspace QSplitter::handle { background: #1c1c1c; }"));
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
    header->addWidget(button(tr("Media"), QStringLiteral("list-add"), QStringLiteral("media"), false));
    header->addWidget(actionButton(QStringLiteral("file_save"), tr("Save"), QStringLiteral("document-save"), false));
    auto *exportButton = actionButton(QStringLiteral("project_render"), tr("Export"), QStringLiteral("document-export"), false);
    exportButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    exportButton->setObjectName(QStringLiteral("mobileExport"));
    header->addWidget(exportButton);
    editor->addLayout(header);

    m_splitter = new QSplitter(Qt::Vertical);
    m_splitter->setObjectName(QStringLiteral("mobileSplitter"));
    m_splitter->setChildrenCollapsible(false);
    m_splitter->setHandleWidth(1);
    auto *monitor = new QWidget;
    auto *monitorLayout = new QVBoxLayout(monitor);
    monitorLayout->setContentsMargins(0, 0, 0, 0);
    monitorLayout->setSpacing(0);
    preview->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    monitorLayout->addWidget(preview, 1);
    auto *transport = new QHBoxLayout;
    transport->setContentsMargins(8, 0, 8, 0);
    auto *leftTransport = new QWidget;
    leftTransport->setFixedWidth(96);
    auto *leftButtons = new QHBoxLayout(leftTransport);
    leftButtons->setContentsMargins(0, 0, 0, 0);
    leftButtons->addWidget(button(tr("Expand preview"), QStringLiteral("view-fullscreen"), QStringLiteral("preview"), false));
    leftButtons->addStretch();
    transport->addWidget(leftTransport);
    transport->addStretch();
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
    enableSwipe(m_panelScroll);
    panelLayout->addWidget(m_panelScroll, 1);
    m_lower->addWidget(m_panelPage);
    m_splitter->addWidget(m_lower);
    m_splitter->setStretchFactor(0, 2);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setSizes({640, 320});
    editor->addWidget(m_splitter, 1);
    m_tools = new QStackedWidget;
    m_tools->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    m_mainTools = toolStrip({
        button(tr("Edit"), QStringLiteral("edit-cut"), QStringLiteral("edit")),
        button(tr("Audio"), QStringLiteral("audio-x-generic"), QStringLiteral("audio")),
        button(tr("Text"), QStringLiteral("draw-text"), QStringLiteral("text")),
        button(tr("Effects"), QStringLiteral("tools-wizard"), QStringLiteral("effects")),
        button(tr("Overlay"), QStringLiteral("overlay"), QStringLiteral("overlay")),
        button(tr("Captions"), QStringLiteral("add-subtitle"), QStringLiteral("captions")),
        button(tr("Filters"), QStringLiteral("filters"), QStringLiteral("filters")),
        button(tr("Adjust"), QStringLiteral("configure"), QStringLiteral("adjust")),
        button(tr("Transitions"), QStringLiteral("view-split-left-right"), QStringLiteral("transitions"))});
    m_editTools = toolStrip({
        button(tr("Back"), QStringLiteral("go-previous"), QStringLiteral("tools")),
        actionButton(QStringLiteral("cut_timeline_clip"), tr("Split"), QStringLiteral("edit-cut")),
        actionButton(QStringLiteral("delete_timeline_selection"), tr("Delete"), QStringLiteral("edit-delete")),
        actionButton(QStringLiteral("duplicate_timeline_clip"), tr("Duplicate"), QStringLiteral("edit-copy")),
        actionButton(QStringLiteral("zoom_fit"), tr("Fit"), QStringLiteral("zoom-fit-best")),
        actionButton(QStringLiteral("view_zoom_out"), tr("Zoom out"), QStringLiteral("zoom-out")),
        actionButton(QStringLiteral("view_zoom_in"), tr("Zoom in"), QStringLiteral("zoom-in"))});
    m_tools->addWidget(m_mainTools);
    m_tools->addWidget(m_editTools);
    m_tools->setFixedHeight(qMax(m_mainTools->height(), m_editTools->height()));
    editor->addWidget(m_tools);
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
    enableSwipe(homeScroll);
    m_pages->addWidget(homeScroll);
    connect(this, &MobileWorkspace::command, this, [this, homeScroll](const QString &name) {
        if (name == QLatin1String("home")) {
            Q_EMIT panelClosed();
            m_pages->setCurrentWidget(homeScroll);
        } else if (name == QLatin1String("continue")) {
            showEditor();
        } else if (name == QLatin1String("done")) {
            Q_EMIT panelClosed();
        } else if (name == QLatin1String("tools")) {
            m_tools->setCurrentWidget(m_mainTools);
        } else if (name == QLatin1String("preview")) {
            const bool expanded = !m_lower->isHidden();
            m_lower->setVisible(!expanded);
            m_tools->setVisible(!expanded);
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
    b->setIcon(touchIcon(icon));
    b->setIconSize(QSize(24, 24));
    b->setToolButtonStyle(label ? Qt::ToolButtonTextUnderIcon : Qt::ToolButtonIconOnly);
    b->setFocusPolicy(Qt::NoFocus);
    if (label) {
        QFont labelFont = font();
        labelFont.setPixelSize(12);
        b->setFont(labelFont);
        b->setStyleSheet(QStringLiteral("min-width: %1px;").arg(qMax(48, b->fontMetrics().horizontalAdvance(text) + 12)));
    }
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
    contents->setObjectName(QStringLiteral("mobileToolContents"));
    auto *row = new QHBoxLayout(contents);
    row->setContentsMargins(8, 4, 8, 4);
    row->setSpacing(2);
    for (QWidget *b : buttons) {
        row->addWidget(b);
    }
    row->addStretch();
    scroll->setWidget(contents);
    // Font/style propagation happens after these controls enter the stacked
    // workspace. Reserve room for both the icon and its label before that.
    scroll->setFixedHeight(qMax(72, contents->sizeHint().height() + 2));
    enableSwipe(scroll);
    return scroll;
}

void MobileWorkspace::showPanel(QWidget *panel, const QString &title, bool media)
{
    m_panelTitle->setText(title);
    m_mediaTools->setVisible(media);
    m_panelScroll->setWidget(panel);
    m_lower->setCurrentWidget(m_panelPage);
    // Opening a tool is an explicit return from expanded preview to editing.
    m_lower->show();
    m_tools->show();
    showEditor();
}

QWidget *MobileWorkspace::takePanel()
{
    QWidget *panel = m_panelScroll->takeWidget();
    m_lower->setCurrentWidget(m_timeline);
    return panel;
}

void MobileWorkspace::showEditor() { m_pages->setCurrentWidget(m_editor); }
void MobileWorkspace::showEditTools() { m_tools->setCurrentWidget(m_editTools); }
void MobileWorkspace::setPlaying(bool playing)
{
    m_play->setIcon(touchIcon(playing ? QStringLiteral("media-playback-pause") : QStringLiteral("media-playback-start")));
    m_play->setAccessibleName(playing ? tr("Pause") : tr("Play"));
    m_play->setToolTip(m_play->accessibleName());
}
void MobileWorkspace::setTimecode(const QString &timecode) { setAccessibleDescription(tr("Playhead %1").arg(timecode)); }
QSize MobileWorkspace::sizeHint() const { return QSize(430, 820); }
QSize MobileWorkspace::minimumSizeHint() const { return QSize(280, 280); }

void MobileWorkspace::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    const bool landscape = width() > height();
    if (landscape != m_landscape) {
        m_landscape = landscape;
        m_splitter->setOrientation(landscape ? Qt::Horizontal : Qt::Vertical);
        m_splitter->setSizes(m_landscape ? QList<int>{600, 400} : QList<int>{640, 320});
    }
}
