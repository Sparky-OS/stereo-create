// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Daniel Campos Ramos <Capitain_Jack@yahoo.com>
#pragma once

#include <QList>
#include <QString>

namespace StereoCreate
{
// What a stereo picture or video can be made into. Each target names its files after the source, next to it.
enum class Target {
    // from a stereo picture (or a frame of a stereo video, the Frame* targets)
    Jps, // JPEG side by side, right view first (cross view), with the JPS descriptor
    Mpo, // CIPA DC-007 Multi-Picture Object, Disparity type, left view first
    AnaglyphRedCyan, // Dubois red/cyan, today's screens
    AnaglyphRedCyanCrt, // Dubois red/cyan, CRT screens
    AnaglyphGreenMagenta,
    AnaglyphAmberBlue,
    SideBySidePng, // PNG side by side, left view first, with the sTER chunk
    Pns, // PNG side by side, right view first (cross view), with the sTER chunk
    ProofPair, // the house proof: MPO and the red/cyan anaglyph
    // from a stereo video
    VideoFullSideBySide, // Matroska, each view at its full size, StereoMode and the frame packing SEI
    VideoHalfSideBySide, // Matroska, both views in the frame of one, as 3D televisions take them
    VideoTopBottom, // Matroska, half top and bottom, as 3D televisions take them
    VideoMp4, // MP4, full side by side, the frame packing SEI and the st3d box
    VideoAnaglyph, // Matroska, Dubois red/cyan, StereoMode anaglyph
    FrameMpo,
    FrameJps,
    FrameAnaglyph,
};

enum class SourceKind {
    Picture,
    Video
};

struct TargetInfo {
    Target target;
    const char *id; // for the command line
    SourceKind kind;
};

const QList<TargetInfo> &targets();
const TargetInfo *targetById(const QString &id);
const TargetInfo &targetInfo(Target target);
// The text of the target in the menu
QString targetText(Target target);
}
