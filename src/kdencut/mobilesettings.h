/* SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL */
#pragma once
class QDialog;
class QRect;

namespace KdenCut {
// Adapt only Project Settings; other desktop dialogs retain their own layouts.
bool adaptProjectSettings(QDialog *dialog, const QRect &available);
}
