/* SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL */
#pragma once
#include <QStringList>
#include <functional>
class QObject;
class QWidget;

namespace KdenCut {
/** Hand an import to the browser while retaining the originating project's lifetime. */
bool chooseBrowserMedia(QWidget *parent, QObject *project, const QString &kind,
                        const std::function<void(const QStringList &)> &selected,
                        const std::function<void()> &serverPicker);
}
