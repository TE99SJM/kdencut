/*
    SPDX-FileCopyrightText: 2026 KdenCut contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "touchmode.h"
#include "mobileworkspace.h"
#include "kdenlivesettings.h"

#include <KColorSchemeManager>

#include <QAbstractItemView>
#include <QAbstractScrollArea>
#include <QApplication>
#include <QCursor>
#include <QDebug>
#include <QDir>
#include <QDialog>
#include <QScreen>
#include <QTimer>
#include <QWizard>
#include <QEvent>
#include <QFile>
#include <QGraphicsView>
#include <QGuiApplication>
#include <QIcon>
#include <QPlainTextEdit>
#include <QScroller>
#include <QScrollerProperties>
#include <QStandardPaths>
#include <QTextEdit>
#include <QStatusBar>
#include <QToolBar>

using namespace KdenCut;

namespace {
constexpr int touchIconSize = 28;
constexpr int touchTrackHeight = 64;
const char *scrollerProperty = "kdencutScroller";
const char *iconSizeProperty = "kdencutOriginalIconSize";
} // namespace

QString TouchMode::colorSchemeId()
{
    return QStringLiteral("KdenCut");
}

TouchMode *TouchMode::instance()
{
    static TouchMode *self = new TouchMode(qApp);
    return self;
}

TouchMode::TouchMode(QObject *parent)
    : QObject(parent)
{
}

qreal TouchMode::sizeMultiplier()
{
    return KdenliveSettings::touchmode() ? 1.35 : 1.0;
}

bool TouchMode::touchEnabled() const
{
    return KdenliveSettings::touchmode();
}

bool TouchMode::cursorVisible() const
{
    return !m_cursorHidden;
}

void TouchMode::initialize(bool forceShowCursor)
{
    if (m_initialized) {
        return;
    }
    m_initialized = true;
    m_sessionShowCursor = forceShowCursor;

    // Must happen before the first KColorSchemeManager::instance() so the scheme is listed.
    installColorScheme();

    // Outside of KDE Plasma there may be no icon theme at all, buttons would then show
    // text only. Fall back to Breeze (dark), which Kdenlive depends on anyway.
    if (!QIcon::hasThemeIcon(QStringLiteral("document-new"))) {
        const QStringList fallbacks = {QStringLiteral("breeze-dark"), QStringLiteral("breeze")};
        const QString previous = QIcon::themeName();
        for (const QString &theme : fallbacks) {
            QIcon::setThemeName(theme);
            if (QIcon::hasThemeIcon(QStringLiteral("document-new"))) {
                break;
            }
            QIcon::setThemeName(previous);
        }
    }

    qApp->installEventFilter(this);

    if (KdenliveSettings::touchmode()) {
        applyStyleSheet();
    }
    if (!m_sessionShowCursor && !KdenliveSettings::showcursor()) {
        hideCursorNow();
    }
}

void TouchMode::installColorScheme()
{
    // Ship the scheme inside the binary and copy it where KColorSchemeManager looks for schemes.
    // A system-wide copy is also installed by CMake, this keeps uninstalled builds working.
    QFile source(QStringLiteral(":/kdencut/KdenCut.colors"));
    if (!source.open(QIODevice::ReadOnly)) {
        qWarning() << "KdenCut: color scheme resource missing";
        return;
    }
    const QByteArray data = source.readAll();
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation));
    if (!dir.mkpath(QStringLiteral("color-schemes")) || !dir.cd(QStringLiteral("color-schemes"))) {
        return;
    }
    QFile target(dir.absoluteFilePath(QStringLiteral("KdenCut.colors")));
    if (target.exists() && target.open(QIODevice::ReadOnly)) {
        if (target.readAll() == data) {
            return;
        }
        target.close();
    }
    if (target.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        target.write(data);
    }
}

void TouchMode::applyLookIfNeeded()
{
    if (KdenliveSettings::kdencutthemeversion() >= LookVersion) {
        return;
    }
    KColorSchemeManager::instance()->activateSchemeId(colorSchemeId());
    if (KdenliveSettings::trackheight() < touchTrackHeight) {
        KdenliveSettings::setTrackheight(touchTrackHeight);
    }
    // CapCut style clip colors: teal video (from scheme), green audio
    if (KdenliveSettings::audioColor().alpha() == 0) {
        KdenliveSettings::setAudioColor(QColor(29, 107, 87));
    }
    KdenliveSettings::setKdencutthemeversion(LookVersion);
    KdenliveSettings::self()->save();
}

void TouchMode::applyStyleSheet()
{
    QFile qss(QStringLiteral(":/kdencut/kdencut-touch.qss"));
    if (qss.open(QIODevice::ReadOnly)) {
        qApp->setStyleSheet(QString::fromUtf8(qss.readAll()));
    }
}

void TouchMode::setTouchEnabled(bool enabled)
{
    if (enabled == KdenliveSettings::touchmode()) {
        return;
    }
    KdenliveSettings::setTouchmode(enabled);
    if (enabled) {
        applyStyleSheet();
        const auto widgets = QApplication::allWidgets();
        for (QWidget *w : widgets) {
            if (auto *area = qobject_cast<QAbstractScrollArea *>(w)) {
                enableTouchScrolling(area);
            } else if (auto *bar = qobject_cast<QToolBar *>(w)) {
                enlargeToolBar(bar);
            }
        }
    } else {
        qApp->setStyleSheet(QString());
        disableTouchScrolling();
        restoreToolBars();
    }
    Q_EMIT touchEnabledChanged(enabled);
}

void TouchMode::hideCursorNow()
{
    if (!m_cursorHidden) {
        QGuiApplication::setOverrideCursor(QCursor(Qt::BlankCursor));
        m_cursorHidden = true;
    }
}

void TouchMode::setCursorVisible(bool visible)
{
    KdenliveSettings::setShowcursor(visible);
    if (visible) {
        m_sessionShowCursor = true;
        if (m_cursorHidden) {
            m_cursorHidden = false;
            // Remove every blank override we may have stacked
            while (QGuiApplication::overrideCursor() && QGuiApplication::overrideCursor()->shape() == Qt::BlankCursor) {
                QGuiApplication::restoreOverrideCursor();
            }
        }
    } else {
        m_sessionShowCursor = false;
        hideCursorNow();
    }
    Q_EMIT cursorVisibleChanged(visible);
}

void TouchMode::enableTouchScrolling(QWidget *widget)
{
    auto *area = qobject_cast<QAbstractScrollArea *>(widget);
    if (!area || area->property(scrollerProperty).toBool()) {
        return;
    }
    // Views where dragging moves content (media bin, effect lists, titler canvas)
    // keep their drag & drop behavior, swiping would break adding clips to the timeline.
    if (auto *view = qobject_cast<QAbstractItemView *>(area)) {
        if (view->dragEnabled()) {
            return;
        }
        view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        view->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    }
    if (qobject_cast<QGraphicsView *>(area)) {
        return;
    }
    if (auto *edit = qobject_cast<QTextEdit *>(area); edit && !edit->isReadOnly()) {
        return;
    }
    if (auto *edit = qobject_cast<QPlainTextEdit *>(area); edit && !edit->isReadOnly()) {
        return;
    }
    QWidget *viewport = area->viewport();
    if (!viewport) {
        return;
    }
    area->setProperty(scrollerProperty, true);
    QScroller::grabGesture(viewport, QScroller::TouchGesture);
    QScroller *scroller = QScroller::scroller(viewport);
    QScrollerProperties props = scroller->scrollerProperties();
    props.setScrollMetric(QScrollerProperties::VerticalOvershootPolicy, QVariant::fromValue(QScrollerProperties::OvershootAlwaysOff));
    props.setScrollMetric(QScrollerProperties::HorizontalOvershootPolicy, QVariant::fromValue(QScrollerProperties::OvershootAlwaysOff));
    props.setScrollMetric(QScrollerProperties::DragStartDistance, 0.003);
    props.setScrollMetric(QScrollerProperties::DecelerationFactor, 0.2);
    scroller->setScrollerProperties(props);
    m_scrollViewports << viewport;
}

void TouchMode::disableTouchScrolling()
{
    for (const QPointer<QWidget> &viewport : std::as_const(m_scrollViewports)) {
        if (viewport) {
            QScroller::ungrabGesture(viewport);
            if (QWidget *area = viewport->parentWidget()) {
                area->setProperty(scrollerProperty, false);
            }
        }
    }
    m_scrollViewports.clear();
}

void TouchMode::enlargeToolBar(QToolBar *toolBar)
{
    if (!toolBar || toolBar->property(iconSizeProperty).isValid()) {
        return;
    }
    // The status bar has a fixed compact height, keep its toolbars small
    for (QWidget *p = toolBar->parentWidget(); p; p = p->parentWidget()) {
        if (qobject_cast<QStatusBar *>(p)) {
            return;
        }
    }
    const QSize current = toolBar->iconSize();
    if (current.height() >= touchIconSize) {
        return;
    }
    toolBar->setProperty(iconSizeProperty, current);
    toolBar->setIconSize(QSize(touchIconSize, touchIconSize));
    m_toolBars << toolBar;
}

void TouchMode::restoreToolBars()
{
    for (const QPointer<QToolBar> &bar : std::as_const(m_toolBars)) {
        if (bar) {
            bar->setIconSize(bar->property(iconSizeProperty).toSize());
            bar->setProperty(iconSizeProperty, QVariant());
        }
    }
    m_toolBars.clear();
}

bool TouchMode::eventFilter(QObject *watched, QEvent *event)
{
    switch (event->type()) {
    case QEvent::Show:
        if (KdenliveSettings::touchmode() && watched->isWidgetType()) {
            if (auto *dialog = qobject_cast<QDialog *>(watched); dialog && !qobject_cast<QWizard *>(dialog)) {
                QTimer::singleShot(0, dialog, [dialog]() {
                    const QRect available = dialog->screen()->availableGeometry();
                    if (available.width() < 1100 || available.width() < available.height()) {
                        MobileWorkspace::fitDialog(dialog, available);
                    }
                });
            }
            if (auto *area = qobject_cast<QAbstractScrollArea *>(watched)) {
                enableTouchScrolling(area);
            } else if (auto *bar = qobject_cast<QToolBar *>(watched)) {
                enlargeToolBar(bar);
            }
        }
        break;
    case QEvent::MouseMove:
    case QEvent::Enter:
        // Some code paths pop override cursors unbalanced; keep the pointer hidden.
        if (m_cursorHidden && !m_sessionShowCursor && QGuiApplication::overrideCursor() == nullptr) {
            m_cursorHidden = false;
            hideCursorNow();
        }
        break;
    default:
        break;
    }
    return QObject::eventFilter(watched, event);
}
