/* SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL */
#include "runtimefeatures.h"
#include "kdenlivesettings.h"

void KdenCut::configureRuntimeFeatures()
{
    if (qEnvironmentVariable("KDENCUT_RUNTIME") != QLatin1String("1")) return;
    const QString decoder = qEnvironmentVariable("KDENCUT_PREVIEW_DECODER");
    KdenliveSettings::setHwDecoding(decoder == QLatin1String("cuda") || decoder == QLatin1String("vaapi") ? decoder : QString());
    KdenliveSettings::setVaapiEnabled(decoder == QLatin1String("vaapi"));
}
