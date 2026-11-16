/* SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL */
#pragma once
#include <QSize>
#include <QString>

namespace KdenCut {
inline QSize phoneAspect(const QString &name)
{
    if (name == QLatin1String("portrait")) return {1080, 1920};
    if (name == QLatin1String("square")) return {1080, 1080};
    if (name == QLatin1String("landscape")) return {1920, 1080};
    return {};
}
inline QString socialPreset(const QString &platform, const QString &encoder)
{
    if (platform != QLatin1String("TikTok") && platform != QLatin1String("Reels") && platform != QLatin1String("Shorts")) return {};
    const QString backend = encoder == QLatin1String("h264_nvenc") ? QStringLiteral("NVIDIA")
        : encoder == QLatin1String("h264_vaapi") ? QStringLiteral("VAAPI") : QStringLiteral("software");
    return QStringLiteral("KdenCut %1 (%2)").arg(platform, backend);
}
}
