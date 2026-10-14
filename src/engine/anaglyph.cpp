// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Daniel Campos Ramos <Capitain_Jack@yahoo.com>
#include "anaglyph.h"

#include <QtMath>

#include <algorithm>
#include <array>
#include <cmath>

namespace StereoCreate
{
namespace
{
// Coefficients as collected at http://chrisjones.id.au/Dubois/Dubois.html and in KWin's anaglyph output
// (drm_egl_layer_surface.cpp); green/magenta and amber/blue as in Dubois's 2009 table and FFmpeg's stereo3d filter.
constexpr DuboisMatrix redCyan = {
    {{0.4154, 0.4710, 0.1669}, {-0.0458, -0.0484, -0.0257}, {-0.0547, -0.0615, 0.0128}},
    {{-0.0109, -0.0364, -0.0060}, {0.3756, 0.7333, 0.0111}, {-0.0651, -0.1287, 1.2971}},
};
constexpr DuboisMatrix redCyanCrt = {
    {{0.456, 0.500, 0.176}, {-0.040, -0.038, -0.016}, {-0.015, -0.021, -0.005}},
    {{-0.043, -0.088, -0.002}, {0.378, 0.734, -0.018}, {-0.072, -0.113, 1.226}},
};
constexpr DuboisMatrix greenMagenta = {
    {{-0.062, -0.158, -0.039}, {0.284, 0.668, 0.143}, {-0.015, -0.027, 0.021}},
    {{0.529, 0.705, 0.024}, {-0.016, -0.015, -0.065}, {0.009, 0.075, 0.937}},
};
constexpr DuboisMatrix amberBlue = {
    {{1.062, -0.205, 0.299}, {-0.026, 0.908, 0.068}, {-0.038, -0.173, 0.022}},
    {{-0.016, -0.123, -0.017}, {0.006, 0.062, -0.017}, {0.094, 0.185, 0.911}},
};

// The transfer in tables: 8-bit code to linear light, and linear light in 1/65535 steps back to an 8-bit code
struct Tables {
    std::array<float, 256> toLinear;
    std::array<uchar, 65536> toCode;
    Tables()
    {
        for (int i = 0; i < 256; ++i) {
            toLinear[i] = float(srgbToLinear(i / 255.0));
        }
        for (int i = 0; i < 65536; ++i) {
            toCode[i] = uchar(std::clamp(qRound(linearToSrgb(i / 65535.0) * 255.0), 0, 255));
        }
    }
};
const Tables &tables()
{
    static const Tables t;
    return t;
}
}

const DuboisMatrix &duboisMatrix(AnaglyphPair pair)
{
    switch (pair) {
    case AnaglyphPair::RedCyanCrt:
        return redCyanCrt;
    case AnaglyphPair::GreenMagenta:
        return greenMagenta;
    case AnaglyphPair::AmberBlue:
        return amberBlue;
    case AnaglyphPair::RedCyan:
        break;
    }
    return redCyan;
}

double srgbToLinear(double encoded)
{
    return encoded <= 0.04045 ? encoded / 12.92 : std::pow((encoded + 0.055) / 1.055, 2.4);
}

double linearToSrgb(double linear)
{
    return linear <= 0.0031308 ? linear * 12.92 : 1.055 * std::pow(linear, 1.0 / 2.4) - 0.055;
}

void anaglyphRow(const uchar *left, const uchar *right, uchar *out, int width, AnaglyphPair pair)
{
    const DuboisMatrix &m = duboisMatrix(pair);
    const Tables &t = tables();
    for (int x = 0; x < width; ++x) {
        const float l[3] = {t.toLinear[left[0]], t.toLinear[left[1]], t.toLinear[left[2]]};
        const float r[3] = {t.toLinear[right[0]], t.toLinear[right[1]], t.toLinear[right[2]]};
        for (int c = 0; c < 3; ++c) {
            const double v =
                m.left[c][0] * l[0] + m.left[c][1] * l[1] + m.left[c][2] * l[2] + m.right[c][0] * r[0] + m.right[c][1] * r[1] + m.right[c][2] * r[2];
            out[c] = t.toCode[std::clamp(qRound(v * 65535.0), 0, 65535)];
        }
        left += 3;
        right += 3;
        out += 3;
    }
}

QImage anaglyph(const QImage &left, const QImage &right, AnaglyphPair pair)
{
    if (left.isNull() || right.isNull()) {
        return {};
    }
    const QImage l = left.convertToFormat(QImage::Format_RGB888);
    QImage r = right.convertToFormat(QImage::Format_RGB888);
    if (r.size() != l.size()) {
        r = r.scaled(l.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    QImage result(l.size(), QImage::Format_RGB888);
    for (int y = 0; y < l.height(); ++y) {
        anaglyphRow(l.constScanLine(y), r.constScanLine(y), result.scanLine(y), l.width(), pair);
    }
    return result;
}
}
