/* SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL */
#include "phonepresets.h"
#include "dialogs/renderwidget.h"
#include "core.h"
#include "doc/kdenlivedoc.h"
#include "renderpresets/renderpresetrepository.hpp"
#include <KLocalizedString>
#include <KMessageBox>
#include <QDateTime>
#include <QDir>
#include <QUuid>

void RenderWidget::startMobileExport(const QString &platform)
{
    const QString preset = KdenCut::socialPreset(platform, qEnvironmentVariable("KDENCUT_EXPORT_ENCODER"));
    if (preset.isEmpty() || !RenderPresetRepository::get()->presetExists(preset)) {
        KMessageBox::error(this, i18n("The phone export preset is unavailable."));
        return;
    }
    const QString folder = qEnvironmentVariable("KDENCUT_RUNTIME") == QLatin1String("1")
        ? QStringLiteral("/media/exports") : pCore->currentDoc()->projectRenderFolder();
    if (!QDir().mkpath(folder)) {
        KMessageBox::error(this, i18n("Could not create the export folder: %1", folder));
        return;
    }
    const QString filename = QStringLiteral("%1-%2-%3.mp4").arg(platform, QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")),
        QUuid::createUuid().toString(QUuid::Id128).left(8));
    setRenderProfile({{QStringLiteral("renderprofile"), preset}, {QStringLiteral("renderurl"), QDir(folder).filePath(filename)},
        {QStringLiteral("rendermode"), QStringLiteral("0")}, {QStringLiteral("renderrescale"), QStringLiteral("0")},
        {QStringLiteral("rendertctype"), QStringLiteral("-1")}, {QStringLiteral("rendertwopass"), QStringLiteral("0")},
        {QStringLiteral("renderplay"), QStringLiteral("0")}, {QStringLiteral("renderstemaudio"), QStringLiteral("0")}});
    if (m_currentProfile != preset) {
        KMessageBox::error(this, i18n("Clear the export preset search and try again."));
        return;
    }
    m_view.video_box->setChecked(true);
    m_view.audio_box->setChecked(true);
    m_view.proxy_render->setChecked(false);
    m_view.embed_subtitles->setChecked(false);
    m_view.guide_multi_box->setChecked(false);
    m_view.aspect_ratio_type->setCurrentIndex(m_view.aspect_ratio_type->findData(QStringLiteral("vertical")));
    refreshParams();
    if (qEnvironmentVariable("KDENCUT_EXPORT_ENCODER") == QLatin1String("h264_vaapi")) {
        m_params.insert(QStringLiteral("vaapi_device"), qEnvironmentVariable("MLT_AVFORMAT_HWACCEL_DEVICE"));
    }
    slotPrepareExport();
}
