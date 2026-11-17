/* SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL */
#pragma once
#include <QApplication>
#include <QEvent>
#include <QTimer>

namespace KdenCut {
// Qt caches palette() values in application stylesheets. Re-polish after KDE
// switches schemes so existing editors change along with newly created ones.
class StylePaletteWatcher : public QObject
{
public:
    explicit StylePaletteWatcher(QObject *owner) : QObject(owner), m_palette(qApp->palette()) { qApp->installEventFilter(this); }
protected:
    bool eventFilter(QObject *, QEvent *event) override
    {
        if (event->type() == QEvent::ApplicationPaletteChange && qApp->palette() != m_palette) {
            m_palette = qApp->palette();
            QTimer::singleShot(0, this, []() {
                if (!qApp->styleSheet().isEmpty()) qApp->setStyleSheet(qApp->styleSheet());
            });
        }
        return false;
    }
private:
    QPalette m_palette;
};
inline void watchStylePalette(QObject *owner) { new StylePaletteWatcher(owner); }
}
