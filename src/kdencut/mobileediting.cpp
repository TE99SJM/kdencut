/* SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL */
#include "mobileediting.h"
#include "phonepresets.h"
#include "core.h"
#include "doc/kdenlivedoc.h"
#include "effects/effectstack/model/effectstackmodel.hpp"
#include "effects/effectstack/model/abstracteffectitem.hpp"
#include "profiles/profilemodel.hpp"
#include "profiles/profilerepository.hpp"
#include "project/projectmanager.h"
#include "timeline2/model/timelineitemmodel.hpp"
#include "timeline2/view/timelinecontroller.h"
#include "timeline2/view/timelinewidget.h"
#include <KLocalizedString>
#include <KMessageBox>

void KdenCut::setMobileAspect(const QString &name, QWidget *parent)
{
    const QSize size = phoneAspect(name);
    if (!size.isValid() || !pCore->currentDoc()) return;
    auto &current = pCore->getCurrentProfile();
    if (current->width() == size.width() && current->height() == size.height()) return;
    // Keep the exact frame-rate fraction: changing the canvas must not retime clips.
    ProfileParam profile(current.get());
    profile.m_width = size.width();
    profile.m_height = size.height();
    profile.m_sample_aspect_num = profile.m_sample_aspect_den = 1;
    profile.m_sar = 1.;
    profile.m_display_aspect_num = size.width();
    profile.m_display_aspect_den = size.height();
    profile.m_dar = double(size.width()) / size.height();
    profile.m_description = i18n("KdenCut %1 × %2", size.width(), size.height());
    QString path = ProfileRepository::get()->findMatchingProfile(&profile);
    if (path.isEmpty()) path = ProfileRepository::get()->saveProfile(&profile);
    if (path.isEmpty() || !pCore->setCurrentProfile(path)) {
        KMessageBox::error(parent, i18n("Could not change the project aspect ratio."));
        return;
    }
    pCore->projectManager()->slotResetProfiles(true);
    pCore->currentDoc()->setModified();
}

bool KdenCut::editMobileVolume(TimelineWidget *timeline, QWidget *parent)
{
    if (!timeline || !timeline->model()) return false;
    const auto model = timeline->model();
    // AV selections contain both halves. Prefer the audio half, even when the
    // video was tapped. Never insert another Volume effect on repeated taps.
    int audio = -1;
    for (int id : timeline->controller()->selection()) {
        if (!model->isClip(id)) continue;
        const auto state = model->getClipState(id).first;
        if (state == PlaylistState::AudioOnly) { audio = id; break; }
    }
    if (audio < 0) {
        KMessageBox::information(parent, i18n("Select an audio clip, or a linked video and audio clip, to adjust its volume."));
        return false;
    }
    if (model->getTrackProperty(model->getClipTrackId(audio), QStringLiteral("kdenlive:locked")).toInt() != 0) {
        KMessageBox::information(parent, i18n("Unlock the audio track before changing its volume."));
        return false;
    }
    const auto stack = model->getClipEffectStackModel(audio);
    if (!stack) return false;
    if (!stack->hasFilter(QStringLiteral("volume"))) {
        timeline->controller()->addEffectToClip(QStringLiteral("volume"), audio);
    } else {
        const int row = stack->effectRow(QStringLiteral("volume"));
        if (row < 0) return false;
        const auto effect = stack->getEffectStackRow(row);
        if (!effect->isAssetEnabled()) {
            Fun undo = []() { return true; };
            Fun redo = []() { return true; };
            effect->markEnabled(true, undo, redo);
            pCore->pushUndo(undo, redo, i18n("Enable volume"));
        }
        stack->setActiveEffect(row);
        timeline->controller()->showAsset(audio);
    }
    return true;
}
