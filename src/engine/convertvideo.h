// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Daniel Campos Ramos <Capitain_Jack@yahoo.com>
#pragma once

#include "photo.h"
#include "probe.h"

#include <QStringList>

#include <atomic>
#include <functional>

namespace StereoCreate
{
// How a long step reports: the fraction done, and a flag that asks it to stop
struct Progress {
    std::function<void(double)> report;
    const std::atomic<bool> *cancel = nullptr;
    bool cancelled() const
    {
        return cancel && cancel->load();
    }
};

// The filters that take both views out of a frame of the source's packing, each scaled to its own shape (the sample
// aspect ratio applied, a half view stretched back), as the labels [l] and [r]. eyeSize receives that shape. Empty
// for a packing they cannot take apart (checkerboard, quincunx, block-laced, multiview).
QString eyesGraph(const Source &source, QSize *eyeSize);

// A stereo video into a video target (VideoFullSideBySide ... VideoAnaglyph), with our FFmpeg (its frame packing
// SEI filter) and MKVToolNix (the StereoMode element), the audio, subtitle and attachment streams kept.
bool convertVideo(const Source &source, Target target, const QString &output, const Progress &progress, QString *error);
// Both views of the frame at time (seconds), at their own shape
bool grabFrame(const Source &source, double time, StereoPair *pair, const Progress &progress, QString *error);

// The programs, overridable for tests and other installations (STEREO_CREATE_FFMPEG, STEREO_CREATE_MKVPROPEDIT)
QString ffmpegProgram();
QString mkvpropeditProgram();
}
