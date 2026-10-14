// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Daniel Campos Ramos <Capitain_Jack@yahoo.com>
#pragma once

#include <QImage>

namespace StereoCreate
{
// The anaglyphs of Eric Dubois's least-squares method ("A projection method to generate anaglyph stereo images",
// ICASSP 2001), mixed in linear light like KWin's anaglyph modes. Red/cyan has two sets: the one measured for
// today's screens (Zhang and McAllister) and the one for CRT screens (Sanders and McAllister), the sets KWin uses;
// green/magenta and amber/blue are Dubois's own, the other two pairs the shared stereo layer reads.
enum class AnaglyphPair {
    RedCyan,
    RedCyanCrt,
    GreenMagenta,
    AmberBlue
};

struct DuboisMatrix {
    // Rows are the anaglyph's red, green and blue; columns the eye's red, green and blue.
    double left[3][3];
    double right[3][3];
};

const DuboisMatrix &duboisMatrix(AnaglyphPair pair);

// sRGB transfer (IEC 61966-2-1), both ways
double srgbToLinear(double encoded);
double linearToSrgb(double linear);

// The anaglyph of a pair, the size of the left view (the right one is scaled to it when they differ), 8 bits a channel.
QImage anaglyph(const QImage &left, const QImage &right, AnaglyphPair pair);
// One row of packed 8-bit RGB, for video frames.
void anaglyphRow(const uchar *left, const uchar *right, uchar *out, int width, AnaglyphPair pair);
}
