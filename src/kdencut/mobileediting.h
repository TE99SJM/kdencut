/* SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL */
#pragma once
#include <QString>
class QWidget;
class TimelineWidget;
namespace KdenCut {
void setMobileAspect(const QString &name, QWidget *parent);
bool editMobileVolume(TimelineWidget *timeline, QWidget *parent);
}
