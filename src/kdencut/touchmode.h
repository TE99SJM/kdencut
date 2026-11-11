/*
    SPDX-FileCopyrightText: 2026 KdenCut contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QList>
#include <QObject>
#include <QPointer>

class QWidget;
class QToolBar;

namespace KdenCut {

/**
 * @brief Central switch for the KdenCut touch experience.
 *
 * - Installs and activates the CapCut-style "KdenCut" color scheme.
 * - Applies the touch stylesheet (larger, rounded controls).
 * - Adds kinetic swipe scrolling to lists, trees and scroll areas.
 * - Enlarges toolbar icons.
 * - Hides the mouse pointer for touch screens (toggleable).
 */
class TouchMode : public QObject
{
    Q_OBJECT

public:
    /** Bump when the default look changes, so existing users get it applied once. */
    static constexpr int LookVersion = 2;
    /** Scheme id understood by KColorSchemeManager (file base name). */
    static QString colorSchemeId();

    static TouchMode *instance();

    /**
     * Call once from main() right after the QApplication and the config file name are set,
     * and before any KColorSchemeManager is created.
     * @param forceShowCursor true when --show-cursor was passed on the command line.
     */
    void initialize(bool forceShowCursor);

    /** Activate the KdenCut look for new users and users upgrading from an older look. */
    void applyLookIfNeeded();

    bool touchEnabled() const;
    bool cursorVisible() const;

    /** Multiplier used for QML sizes (timeline handles, track heads...). Read at startup. */
    static qreal sizeMultiplier();

public Q_SLOTS:
    void setTouchEnabled(bool enabled);
    void setCursorVisible(bool visible);

Q_SIGNALS:
    void touchEnabledChanged(bool enabled);
    void cursorVisibleChanged(bool visible);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    explicit TouchMode(QObject *parent = nullptr);
    void installColorScheme();
    void applyStyleSheet();
    void enableTouchScrolling(QWidget *widget);
    void disableTouchScrolling();
    void enlargeToolBar(QToolBar *toolBar);
    void restoreToolBars();
    void hideCursorNow();

    bool m_initialized{false};
    bool m_cursorHidden{false};
    bool m_sessionShowCursor{false};
    QList<QPointer<QWidget>> m_scrollViewports;
    QList<QPointer<QToolBar>> m_toolBars;
};

} // namespace KdenCut
