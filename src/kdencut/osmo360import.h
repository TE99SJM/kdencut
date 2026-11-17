// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QString>

class QWidget;

namespace KdenCut {
// Returns a rendered clip, the original when explicitly requested, or empty on cancel.
QString prepareOsmo360Clip(const QString &source, QWidget *parent);
}
