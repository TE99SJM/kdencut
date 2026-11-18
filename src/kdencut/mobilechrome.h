/* SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL */
#pragma once

#include <QApplication>
#include <QEvent>
#include <QMainWindow>
#include <QMenuBar>
#include <QPointer>
#include <QStatusBar>
#include <QToolBar>
#include <utility>

namespace KdenCut {
/** Keep desktop window controls out of the phone workspace, including when
 * KXmlGuiWindow restores saved toolbar visibility after startup. Embedded panel
 * controls and other windows are unaffected. Destroy before returning to desktop.
 */
class MobileChrome final : public QObject
{
public:
    explicit MobileChrome(QMainWindow *window, QObject *parent)
        : QObject(parent)
        , m_window(window)
    {
        qApp->installEventFilter(this);
        for (QWidget *widget : window->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly)) {
            if (isDesktopControl(widget)) suppress(widget);
        }
    }

    ~MobileChrome() override
    {
        qApp->removeEventFilter(this);
        if (m_window) {
            for (const auto &entry : std::as_const(m_controls)) {
                if (entry.widget) entry.widget->setVisible(entry.visible);
            }
        }
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::Show || event->type() == QEvent::ShowToParent) {
            if (auto *widget = qobject_cast<QWidget *>(watched); widget && isDesktopControl(widget)) {
                suppress(widget);
            }
        }
        return false;
    }

private:
    bool isDesktopControl(QWidget *widget) const
    {
        return m_window && widget->parentWidget() == m_window
            && (qobject_cast<QToolBar *>(widget) || qobject_cast<QMenuBar *>(widget)
                || qobject_cast<QStatusBar *>(widget) || widget == m_window->menuWidget());
    }

    void suppress(QWidget *widget)
    {
        bool known = false;
        for (const auto &entry : std::as_const(m_controls)) {
            if (entry.widget == widget) { known = true; break; }
        }
        // Record hidden controls too: a restore/show attempt must not change
        // which bars the user originally chose to display on the desktop.
        if (!known) m_controls.append({widget, !widget->isHidden()});
        if (!widget->isHidden()) widget->hide();
    }

    struct Control { QPointer<QWidget> widget; bool visible; };
    QPointer<QMainWindow> m_window;
    QList<Control> m_controls;
};
} // namespace KdenCut
