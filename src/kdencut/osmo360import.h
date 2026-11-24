// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QString>
#include <functional>

class QMainWindow;

namespace KdenCut {
// Called on the GUI thread. Return immediately, then deliver a successful result
// on that same thread while the original import destination still exists.
void queueOsmo360Clip(QMainWindow *window, const QString &source, std::function<bool()> valid, std::function<void(const QString &)> imported);
}
