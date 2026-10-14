// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Daniel Campos Ramos <Capitain_Jack@yahoo.com>
#include "targets.h"

#include <KLocalizedString>

namespace StereoCreate
{
const QList<TargetInfo> &targets()
{
    static const QList<TargetInfo> list = {
        {Target::Jps, "jps", SourceKind::Picture},
        {Target::Mpo, "mpo", SourceKind::Picture},
        {Target::AnaglyphRedCyan, "anaglyph-red-cyan", SourceKind::Picture},
        {Target::AnaglyphRedCyanCrt, "anaglyph-red-cyan-crt", SourceKind::Picture},
        {Target::AnaglyphGreenMagenta, "anaglyph-green-magenta", SourceKind::Picture},
        {Target::AnaglyphAmberBlue, "anaglyph-amber-blue", SourceKind::Picture},
        {Target::SideBySidePng, "sbs-png", SourceKind::Picture},
        {Target::Pns, "pns", SourceKind::Picture},
        {Target::ProofPair, "proof", SourceKind::Picture},
        {Target::VideoFullSideBySide, "video-full-sbs", SourceKind::Video},
        {Target::VideoHalfSideBySide, "video-half-sbs", SourceKind::Video},
        {Target::VideoTopBottom, "video-top-bottom", SourceKind::Video},
        {Target::VideoMp4, "video-mp4", SourceKind::Video},
        {Target::VideoAnaglyph, "video-anaglyph", SourceKind::Video},
        {Target::FrameMpo, "frame-mpo", SourceKind::Video},
        {Target::FrameJps, "frame-jps", SourceKind::Video},
        {Target::FrameAnaglyph, "frame-anaglyph", SourceKind::Video},
    };
    return list;
}

const TargetInfo *targetById(const QString &id)
{
    for (const TargetInfo &info : targets()) {
        if (id == QLatin1String(info.id)) {
            return &info;
        }
    }
    return nullptr;
}

const TargetInfo &targetInfo(Target target)
{
    for (const TargetInfo &info : targets()) {
        if (info.target == target) {
            return info;
        }
    }
    return targets().constFirst();
}

QString targetText(Target target)
{
    switch (target) {
    case Target::Jps:
        return i18nc("@action:inmenu Create stereo", "JPS (cross view)");
    case Target::Mpo:
        return i18nc("@action:inmenu Create stereo", "MPO");
    case Target::AnaglyphRedCyan:
        return i18nc("@action:inmenu Create stereo", "Red/Cyan Anaglyph");
    case Target::AnaglyphRedCyanCrt:
        return i18nc("@action:inmenu Create stereo", "Red/Cyan Anaglyph for CRT Screens");
    case Target::AnaglyphGreenMagenta:
        return i18nc("@action:inmenu Create stereo", "Green/Magenta Anaglyph");
    case Target::AnaglyphAmberBlue:
        return i18nc("@action:inmenu Create stereo", "Amber/Blue Anaglyph");
    case Target::SideBySidePng:
        return i18nc("@action:inmenu Create stereo", "Side by Side PNG");
    case Target::Pns:
        return i18nc("@action:inmenu Create stereo", "PNS (cross view)");
    case Target::ProofPair:
        return i18nc("@action:inmenu Create stereo", "Proof Pair (MPO and Red/Cyan Anaglyph)");
    case Target::VideoFullSideBySide:
        return i18nc("@action:inmenu Create stereo", "Full Side by Side Video (MKV)");
    case Target::VideoHalfSideBySide:
        return i18nc("@action:inmenu Create stereo", "Half Side by Side Video for 3D TVs (MKV)");
    case Target::VideoTopBottom:
        return i18nc("@action:inmenu Create stereo", "Top and Bottom Video for 3D TVs (MKV)");
    case Target::VideoMp4:
        return i18nc("@action:inmenu Create stereo", "Side by Side Video (MP4)");
    case Target::VideoAnaglyph:
        return i18nc("@action:inmenu Create stereo", "Red/Cyan Anaglyph Video");
    case Target::FrameMpo:
        return i18nc("@action:inmenu Create stereo", "Frame as MPO");
    case Target::FrameJps:
        return i18nc("@action:inmenu Create stereo", "Frame as JPS");
    case Target::FrameAnaglyph:
        return i18nc("@action:inmenu Create stereo", "Frame as Red/Cyan Anaglyph");
    }
    return {};
}
}
