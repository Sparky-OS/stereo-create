// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Daniel Campos Ramos <Capitain_Jack@yahoo.com>
#pragma once

#include "targets.h"

#include <picture.h>

#include <QSize>
#include <QString>
#include <QStringList>

namespace StereoCreate
{
// What a file holds, decided by the shared policy (stereo-layer): a picture by its mark, its format or its shape; a
// video by its stream's mark (Matroska StereoMode and the st3d box, as the demuxer reads them, then the frame packing
// SEI of its first frame), then the frame's shape. The user's swap-eyes correction kept with the file applies over
// the mark.
struct Source {
    QString path;
    SourceKind kind = SourceKind::Picture;
    bool stereo = false;
    Stereo::Picture::Layout layout = Stereo::Picture::Layout::None;
    Stereo::Picture::EyeOrder order = Stereo::Picture::EyeOrder::LeftFirst;
    Stereo::Picture::Source origin = Stereo::Picture::Source::None;
    // videos
    QString mark; // the stream's own, a Stereo3DLayout word; empty without one
    QSize frameSize; // the coded frame
    QSize displaySize; // the frame as shown, its sample aspect ratio applied
    bool fullViews = false; // each view at its own shape (or squeezed into half the frame)
    double duration = 0; // seconds
    QString codec; // FFmpeg's codec name
    int bitDepth = 8;
    // The colour description, FFmpeg's names ("bt2020", "smpte2084", "bt2020nc", "tv"); empty when unknown
    QString colorPrimaries;
    QString colorTransfer;
    QString colorSpace;
    QString colorRange;
    int fpsNum = 0; // the average frame rate
    int fpsDen = 1;
    QStringList audioCodecs; // FFmpeg's codec names, one per audio stream
};

bool isVideo(const QString &path);
bool isPicture(const QString &path);
// quick: what a context menu can afford (a picture is decoded small, only when it can be a pair)
Source probe(const QString &path, bool quick = false);
// The Stereo3DLayout word of an FFmpeg Stereo 3D type and its "inverted" flag (libavutil/stereo3d.h)
QString markForStereo3D(int type, bool inverted);
}
