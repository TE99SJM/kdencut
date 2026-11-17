/* SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL */
#include "runtimefeatures.h"
#include "kdenlivesettings.h"
#include <QFileInfo>

void KdenCut::configureRuntimeFeatures()
{
    if (qEnvironmentVariable("KDENCUT_RUNTIME") != QLatin1String("1")) return;
    const QString decoder = qEnvironmentVariable("KDENCUT_PREVIEW_DECODER");
    KdenliveSettings::setHwDecoding(decoder == QLatin1String("cuda") || decoder == QLatin1String("vaapi") ? decoder : QString());
    KdenliveSettings::setVaapiEnabled(decoder == QLatin1String("vaapi"));
    const QString models = qEnvironmentVariable("KDENCUT_WHISPER_MODELS");
    if (KdenliveSettings::kdencutspeechversion() < 1 && QFileInfo::exists(models + QStringLiteral("/tiny.pt"))) {
        KdenliveSettings::setSpeech_system_python(true);
        KdenliveSettings::setSpeech_system_python_path(QStringLiteral("/opt/kdencut-speech/bin/python3"));
        KdenliveSettings::setSpeechEngine(QStringLiteral("whisper"));
        KdenliveSettings::setWhisperModelFolder(models);
        KdenliveSettings::setWhisperModel(QStringLiteral("tiny"));
        KdenliveSettings::setWhisperDevice(QStringLiteral("cpu"));
        KdenliveSettings::setWhisperDisableFP16(true);
        KdenliveSettings::setSubtitleMode(1);
        KdenliveSettings::setKdencutspeechversion(1);
    }
}
