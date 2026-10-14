// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Daniel Campos Ramos <Capitain_Jack@yahoo.com>
#pragma once

#include "anaglyph.h"

#include <QImage>
#include <QString>

namespace StereoCreate
{
struct StereoPair {
    QImage left;
    QImage right;
    bool isValid() const
    {
        return !left.isNull() && !right.isNull();
    }
};

// The JPEG quality of the JPS, MPO and anaglyph files
constexpr int DefaultQuality = 92;

// JPS: one JPEG, the right view on the left (cross view), written by KImageFormats' stereo writer, with the JPS
// descriptor (APP3 "_JPSJPS_": stereo, side by side, right view first, full width) that JPS readers look for.
bool writeJps(const StereoPair &pair, const QString &path, int quality, QString *error);
// MPO: the left view and the right view as the two pictures of a CIPA DC-007 Multi-Picture Object, type Disparity,
// each with its MP Attribute IFD (MP Individual Image Number 1 and 2, Base Viewpoint Number 1), written by
// KImageFormats' stereo writer.
bool writeMpo(const StereoPair &pair, const QString &path, int quality, QString *error);
// PNG side by side with the sTER chunk (PNG Extensions, sTER): leftFirst writes mode 1 (the left view on the left),
// otherwise mode 0 (cross view, as PNS files are). The right picture starts on a multiple of 8 columns, after the
// padding the chunk defines.
bool writeSideBySidePng(const StereoPair &pair, const QString &path, bool leftFirst, QString *error);
// The JPS descriptor and the sTER chunk on their own, for the tests
QByteArray jpsDescriptorSegment();
QByteArray pngChunk(const QByteArray &type, const QByteArray &data);
int sterPadding(int viewWidth);
// An anaglyph, JPEG unless the path ends in .png
bool writeAnaglyph(const StereoPair &pair, AnaglyphPair colours, const QString &path, int quality, QString *error);

// A path for a new file next to source: its name with suffix and the extension, never an existing file (then
// "name (1).ext", as KIO names copies).
QString outputPath(const QString &source, const QString &suffix, const QString &extension);
}
