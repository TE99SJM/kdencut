/* SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL */
#pragma once

#include <QAbstractScrollArea>
#include <QScroller>
#include <QScrollerProperties>
#include <QVariant>

namespace KdenCut {
// Selkies translates finger movement into mouse drags. Native Qt sessions can
// still use touch gestures; do not install this on drag/drop editing surfaces.
inline void enableSwipeScrolling(QAbstractScrollArea *area, bool remoteMouse)
{
    area->setProperty("kdencutScroller", true);
    QScroller::grabGesture(area->viewport(), remoteMouse ? QScroller::LeftMouseButtonGesture : QScroller::TouchGesture);
    auto *scroller = QScroller::scroller(area->viewport());
    auto properties = scroller->scrollerProperties();
    properties.setScrollMetric(QScrollerProperties::FrameRate, QScrollerProperties::Fps60);
    properties.setScrollMetric(QScrollerProperties::DragStartDistance, 0.002);
    properties.setScrollMetric(QScrollerProperties::MousePressEventDelay, 0.1);
    properties.setScrollMetric(QScrollerProperties::DecelerationFactor, 0.12);
    properties.setScrollMetric(QScrollerProperties::HorizontalOvershootPolicy, QScrollerProperties::OvershootAlwaysOff);
    properties.setScrollMetric(QScrollerProperties::VerticalOvershootPolicy, QScrollerProperties::OvershootAlwaysOff);
    scroller->setScrollerProperties(properties);
}
}
