// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Daniel Campos Ramos <Capitain_Jack@yahoo.com>
#include "photo.h"

#include <KFileUtils>
#include <KLocalizedString>

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageWriter>
#include <QPainter>
#include <QSaveFile>
#include <QUrl>
#include <QtEndian>

namespace StereoCreate
{
namespace
{
// The pair side by side, left view first, both at the left view's size
QImage sideBySide(const StereoPair &pair)
{
    QImage right = pair.right;
    if (right.size() != pair.left.size()) {
        right = right.scaled(pair.left.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    QImage image(pair.left.width() * 2, pair.left.height(), QImage::Format_RGB888);
    QPainter painter(&image);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.drawImage(0, 0, pair.left.convertToFormat(QImage::Format_RGB888));
    painter.drawImage(pair.left.width(), 0, right.convertToFormat(QImage::Format_RGB888));
    return image;
}

bool encode(const QImage &image, const QByteArray &format, int quality, QByteArray *data, QString *error)
{
    QBuffer buffer(data);
    buffer.open(QIODevice::WriteOnly);
    QImageWriter writer(&buffer, format);
    writer.setQuality(quality);
    if (!writer.write(image)) {
        if (error) {
            *error =
                i18nc("@info %1 is a file format, %2 an error message", "Cannot write %1: %2", QString::fromLatin1(format).toUpper(), writer.errorString());
        }
        return false;
    }
    return true;
}

bool save(const QString &path, const QByteArray &data, QString *error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        if (error) {
            *error = i18nc("@info %1 is a file name, %2 an error message", "Cannot save %1: %2", path, file.errorString());
        }
        return false;
    }
    return true;
}

// Where a new JPEG marker segment goes: after SOI and the APP0 and APP1 segments (JFIF, Exif)
qsizetype jpegInsertionPoint(const QByteArray &jpeg)
{
    if (jpeg.size() < 4 || uchar(jpeg[0]) != 0xff || uchar(jpeg[1]) != 0xd8) {
        return -1;
    }
    qsizetype offset = 2;
    while (offset + 4 <= jpeg.size() && uchar(jpeg[offset]) == 0xff && (uchar(jpeg[offset + 1]) == 0xe0 || uchar(jpeg[offset + 1]) == 0xe1)) {
        const quint16 length = qFromBigEndian<quint16>(reinterpret_cast<const uchar *>(jpeg.constData() + offset + 2));
        offset += 2 + length;
    }
    return offset <= jpeg.size() ? offset : -1;
}
}

QByteArray jpsDescriptorSegment()
{
    // JPS: APP3, "_JPSJPS_", the length of the descriptor (4) and the descriptor, big endian: MEDIA_TYPE 1 (stereo)
    // in the low byte, layout 2 (side by side) in the next, flags 0 (full width and height, right view first)
    QByteArray payload("_JPSJPS_");
    payload.append(char(0)).append(char(4));
    const quint32 descriptor = 0x01 | (0x02 << 8);
    payload.append(char(descriptor >> 24)).append(char(descriptor >> 16)).append(char(descriptor >> 8)).append(char(descriptor));
    QByteArray segment;
    segment.append(char(0xff)).append(char(0xe3));
    const quint16 length = quint16(payload.size() + 2);
    segment.append(char(length >> 8)).append(char(length & 0xff));
    return segment + payload;
}

bool writeJps(const StereoPair &pair, const QString &path, int quality, QString *error)
{
    QByteArray data;
    if (!encode(sideBySide(pair), "jps", quality, &data, error)) {
        return false;
    }
    const qsizetype at = jpegInsertionPoint(data);
    if (at < 0) {
        if (error) {
            *error = i18nc("@info", "The JPS writer gave no JPEG data.");
        }
        return false;
    }
    data.insert(at, jpsDescriptorSegment());
    return save(path, data, error);
}

bool writeMpo(const StereoPair &pair, const QString &path, int quality, QString *error)
{
    QByteArray data;
    return encode(sideBySide(pair), "mpo", quality, &data, error) && save(path, data, error);
}

int sterPadding(int viewWidth)
{
    return (8 - viewWidth % 8) % 8;
}

QByteArray pngChunk(const QByteArray &type, const QByteArray &data)
{
    QByteArray chunk;
    const quint32 length = data.size();
    chunk.append(char(length >> 24)).append(char(length >> 16)).append(char(length >> 8)).append(char(length));
    const QByteArray body = type + data;
    chunk.append(body);
    quint32 crc = 0xffffffff;
    for (const char c : body) {
        crc ^= quint8(c);
        for (int k = 0; k < 8; ++k) {
            crc = (crc & 1) ? 0xedb88320 ^ (crc >> 1) : crc >> 1;
        }
    }
    crc ^= 0xffffffff;
    chunk.append(char(crc >> 24)).append(char(crc >> 16)).append(char(crc >> 8)).append(char(crc));
    return chunk;
}

bool writeSideBySidePng(const StereoPair &pair, const QString &path, bool leftFirst, QString *error)
{
    QImage right = pair.right;
    if (right.size() != pair.left.size()) {
        right = right.scaled(pair.left.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    const bool alpha = pair.left.hasAlphaChannel() || right.hasAlphaChannel();
    const int width = pair.left.width();
    const int padding = sterPadding(width);
    QImage image(width * 2 + padding, pair.left.height(), alpha ? QImage::Format_ARGB32 : QImage::Format_RGB888);
    image.fill(alpha ? Qt::transparent : Qt::black);
    QPainter painter(&image);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.drawImage(0, 0, leftFirst ? pair.left : right);
    painter.drawImage(width + padding, 0, leftFirst ? right : pair.left);
    painter.end();
    QByteArray data;
    if (!encode(image, "png", -1, &data, error)) {
        return false;
    }
    // sTER goes before the first IDAT: right after IHDR (the signature, 8 bytes, and IHDR, 25)
    data.insert(8 + 25, pngChunk("sTER", QByteArray(1, leftFirst ? '\x01' : '\x00')));
    return save(path, data, error);
}

bool writeAnaglyph(const StereoPair &pair, AnaglyphPair colours, const QString &path, int quality, QString *error)
{
    const QImage image = anaglyph(pair.left, pair.right, colours);
    QByteArray data;
    const QByteArray format = path.endsWith(QLatin1String(".png"), Qt::CaseInsensitive) ? QByteArrayLiteral("png") : QByteArrayLiteral("jpeg");
    return encode(image, format, format == "png" ? -1 : quality, &data, error) && save(path, data, error);
}

QString outputPath(const QString &source, const QString &suffix, const QString &extension)
{
    const QFileInfo info(source);
    const QString name = info.completeBaseName() + suffix + QLatin1Char('.') + extension;
    const QDir dir = info.absoluteDir();
    if (!dir.exists(name)) {
        return dir.filePath(name);
    }
    return dir.filePath(KFileUtils::suggestName(QUrl::fromLocalFile(dir.absolutePath() + QLatin1Char('/')), name));
}
}
