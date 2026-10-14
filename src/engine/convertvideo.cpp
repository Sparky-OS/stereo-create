// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Daniel Campos Ramos <Capitain_Jack@yahoo.com>
#include "convertvideo.h"

#include <KLocalizedString>

#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>
#include <QtEndian>

namespace StereoCreate
{
namespace
{
using Layout = Stereo::Picture::Layout;
using Order = Stereo::Picture::EyeOrder;

int even(double value)
{
    return qMax(2, int(value / 2 + 0.5) * 2);
}

QString text(const QByteArray &data)
{
    return QString::fromLocal8Bit(data).trimmed();
}

// Run FFmpeg with "-progress pipe:1": out_time against the duration is the fraction done. Stops on cancel.
bool runFfmpeg(const QStringList &arguments, double duration, const Progress &progress, QString *error)
{
    QProcess process;
    process.setProgram(ffmpegProgram());
    process.setArguments(QStringList{QStringLiteral("-hide_banner"),
                                     QStringLiteral("-nostdin"),
                                     QStringLiteral("-y"),
                                     QStringLiteral("-v"),
                                     QStringLiteral("error"),
                                     QStringLiteral("-progress"),
                                     QStringLiteral("pipe:1"),
                                     QStringLiteral("-nostats")}
                         + arguments);
    process.start();
    if (!process.waitForStarted()) {
        if (error) {
            *error = i18nc("@info %1 is a program", "Cannot start %1.", process.program());
        }
        return false;
    }
    QByteArray pending;
    while (process.state() != QProcess::NotRunning) {
        if (progress.cancelled()) {
            process.kill();
            process.waitForFinished();
            return false;
        }
        process.waitForReadyRead(100);
        pending += process.readAllStandardOutput();
        qsizetype end;
        while ((end = pending.indexOf('\n')) >= 0) {
            const QByteArray line = pending.left(end);
            pending.remove(0, end + 1);
            if (line.startsWith("out_time_us=") && duration > 0 && progress.report) {
                const double seconds = line.mid(12).toDouble() / 1e6;
                progress.report(qBound(0.0, seconds / duration, 1.0));
            }
        }
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (error) {
            *error = i18nc("@info %1 is an error message", "FFmpeg failed: %1", text(process.readAllStandardError()));
        }
        return false;
    }
    return true;
}

bool run(const QString &program, const QStringList &arguments, QString *error)
{
    QProcess process;
    process.start(program, arguments);
    if (!process.waitForStarted() || !process.waitForFinished(-1) || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (error) {
            *error = i18nc("@info %1 is a program, %2 its message",
                           "%1 failed: %2",
                           program,
                           text(process.readAllStandardError() + process.readAllStandardOutput()));
        }
        return false;
    }
    return true;
}

// MP4 boxes (ISO/IEC 14496-12): the size, 64 bits when it reads 1, and the type
struct Box {
    qint64 size = 0;
    int header = 8;
    QByteArray type;
};

Box readBox(const QByteArray &data, qint64 at)
{
    Box box;
    if (at + 8 > data.size()) {
        return box;
    }
    const auto *p = reinterpret_cast<const uchar *>(data.constData() + at);
    box.size = qFromBigEndian<quint32>(p);
    box.type = data.mid(at + 4, 4);
    if (box.size == 1 && at + 16 <= data.size()) {
        box.size = qint64(qFromBigEndian<quint64>(p + 8));
        box.header = 16;
    } else if (box.size == 0) {
        box.size = data.size() - at;
    }
    return box;
}

void addToSize(QByteArray &data, qint64 at, const Box &box, qint64 delta)
{
    auto *p = reinterpret_cast<uchar *>(data.data() + at);
    if (box.header == 16) {
        qToBigEndian<quint64>(quint64(box.size + delta), p + 8);
    } else {
        qToBigEndian<quint32>(quint32(box.size + delta), p);
    }
}

// Remove every box of this type from the visual sample entries under moov, fixing the sizes of the boxes around it;
// the number of bytes removed
qint64 removeFromSampleEntries(QByteArray &data, qint64 start, qint64 end, const QByteArray &type, int depth)
{
    static const QList<QByteArray> containers = {"moov", "trak", "mdia", "minf", "stbl"};
    static const QList<QByteArray> sampleEntries = {"avc1", "avc3", "hvc1", "hev1"};
    qint64 removed = 0;
    qint64 at = start;
    while (at + 8 <= end - removed) {
        const Box box = readBox(data, at);
        if (box.size < box.header || at + box.size > end - removed) {
            break;
        }
        qint64 inner = -1;
        if (containers.contains(box.type)) {
            inner = box.header;
        } else if (box.type == "stsd") {
            inner = box.header + 8; // version, flags, entry count
        } else if (depth > 0 && sampleEntries.contains(box.type)) {
            inner = box.header + 78; // the VisualSampleEntry fields
        } else if (depth > 1 && box.type == type) {
            data.remove(at, box.size);
            removed += box.size;
            continue;
        }
        if (inner >= 0) {
            const qint64 less =
                removeFromSampleEntries(data, at + inner, at + box.size, type, sampleEntries.contains(box.type) ? 2 : (box.type == "stsd" ? 1 : depth));
            if (less) {
                addToSize(data, at, box, -less);
                removed += less;
            }
            at += box.size - less;
        } else {
            at += box.size;
        }
    }
    return removed;
}

// FFmpeg's MP4 muxer writes Apple's Video Extended Usage box (vexu, for multiview video) next to st3d whenever a
// stream has Stereo 3D side data; for a frame-packed stream it only says that two views exist and readers take it
// over st3d. The box is removed: st3d (Spherical Video V2) and the frame packing SEI describe the frame. The moov box
// must come last (no "faststart"), so that no chunk offset moves.
bool removeVexu(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadWrite)) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }
    qint64 at = 0;
    qint64 moov = -1;
    while (at + 8 <= file.size()) {
        file.seek(at);
        const QByteArray header = file.read(16);
        const auto *p = reinterpret_cast<const uchar *>(header.constData());
        qint64 size = qFromBigEndian<quint32>(p);
        if (size == 1 && header.size() == 16) {
            size = qint64(qFromBigEndian<quint64>(p + 8));
        } else if (size == 0) {
            size = file.size() - at;
        }
        if (size < 8) {
            break;
        }
        if (header.mid(4, 4) == "moov") {
            moov = at;
        }
        at += size;
    }
    if (moov < 0) {
        return true;
    }
    file.seek(moov);
    QByteArray data = file.readAll();
    if (moov + data.size() != file.size()) {
        return true;
    }
    if (removeFromSampleEntries(data, 0, data.size(), QByteArrayLiteral("vexu"), 0) == 0) {
        return true;
    }
    return file.resize(moov) && file.seek(moov) && file.write(data) == data.size();
}

bool hevc(const Source &source)
{
    return source.codec == QLatin1String("hevc");
}

// The encoder: H.265 stays H.265, anything else becomes H.264, both with a frame packing SEI the displays read
QStringList encoder(const Source &source)
{
    if (hevc(source)) {
        return {QStringLiteral("-c:v"),
                QStringLiteral("libx265"),
                QStringLiteral("-crf"),
                QStringLiteral("20"),
                QStringLiteral("-preset"),
                QStringLiteral("medium"),
                QStringLiteral("-x265-params"),
                QStringLiteral("log-level=error")};
    }
    return {QStringLiteral("-c:v"),
            QStringLiteral("libx264"),
            QStringLiteral("-crf"),
            QStringLiteral("18"),
            QStringLiteral("-preset"),
            QStringLiteral("medium")};
}

// The source's colour description on the frames and in the stream (VUI): primaries, transfer, matrix and range,
// with the bit depth the encoder keeps, so deep colour and HDR stay as they were
QString colourDescription(const Source &source)
{
    QStringList parameters;
    if (!source.colorPrimaries.isEmpty()) {
        parameters.append(QStringLiteral("color_primaries=") + source.colorPrimaries);
    }
    if (!source.colorTransfer.isEmpty()) {
        parameters.append(QStringLiteral("color_trc=") + source.colorTransfer);
    }
    if (!source.colorSpace.isEmpty()) {
        parameters.append(QStringLiteral("colorspace=") + source.colorSpace);
    }
    if (!source.colorRange.isEmpty()) {
        parameters.append(QStringLiteral("range=") + source.colorRange);
    }
    return parameters.isEmpty() ? QString() : QStringLiteral(",setparams=") + parameters.join(QLatin1Char(':'));
}

QStringList colourOptions(const Source &source)
{
    QStringList options;
    if (!source.colorPrimaries.isEmpty()) {
        options += {QStringLiteral("-color_primaries"), source.colorPrimaries};
    }
    if (!source.colorTransfer.isEmpty()) {
        options += {QStringLiteral("-color_trc"), source.colorTransfer};
    }
    if (!source.colorSpace.isEmpty()) {
        options += {QStringLiteral("-colorspace"), source.colorSpace};
    }
    if (!source.colorRange.isEmpty()) {
        options += {QStringLiteral("-color_range"), source.colorRange};
    }
    return options;
}

// The frame packing SEI (H.264 Table D-8, H.265 Table D.8), through the edition's frame_packing bitstream filter
QStringList framePackingSei(const Source &source, const QString &type)
{
    return {QStringLiteral("-bsf:v"),
            QStringLiteral("%1=frame_packing=insert:frame_packing_type=%2:frame_packing_view=left_first")
                .arg(hevc(source) ? QStringLiteral("hevc_metadata") : QStringLiteral("h264_metadata"), type)};
}

// The streams that are not the picture, kept as they are
QStringList otherStreams(int input)
{
    const QString n = QString::number(input);
    return {QStringLiteral("-map"),
            n + QStringLiteral(":a?"),
            QStringLiteral("-map"),
            n + QStringLiteral(":s?"),
            QStringLiteral("-map"),
            n + QStringLiteral(":t?"),
            QStringLiteral("-c:a"),
            QStringLiteral("copy"),
            QStringLiteral("-c:s"),
            QStringLiteral("copy"),
            QStringLiteral("-c:t"),
            QStringLiteral("copy")};
}

// RFC 9559 StereoMode, set by MKVToolNix
bool setStereoMode(const QString &path, int mode, QString *error)
{
    return run(mkvpropeditProgram(),
               {path, QStringLiteral("--edit"), QStringLiteral("track:v1"), QStringLiteral("--set"), QStringLiteral("stereo-mode=%1").arg(mode)},
               error);
}

QString temporaryName(const QString &output, const QString &extension)
{
    const QFileInfo info(output);
    return info.absolutePath() + QStringLiteral("/.") + info.completeBaseName() + QStringLiteral(".part.") + extension;
}

// The anaglyph: FFmpeg decodes both views side by side into raw RGB, each frame is mixed here (anaglyph.h), and a
// second FFmpeg encodes the result with the source's other streams.
bool convertAnaglyph(const Source &source, const QString &graph, QSize eye, const QString &output, const Progress &progress, QString *error)
{
    if (source.fpsNum <= 0) {
        if (error) {
            *error = i18nc("@info", "The video has no frame rate to keep.");
        }
        return false;
    }
    QProcess decoder;
    decoder.setProgram(ffmpegProgram());
    decoder.setArguments({QStringLiteral("-hide_banner"),
                          QStringLiteral("-nostdin"),
                          QStringLiteral("-v"),
                          QStringLiteral("error"),
                          QStringLiteral("-i"),
                          source.path,
                          QStringLiteral("-filter_complex"),
                          graph + QStringLiteral(";[l][r]hstack,format=rgb24[v]"),
                          QStringLiteral("-map"),
                          QStringLiteral("[v]"),
                          QStringLiteral("-f"),
                          QStringLiteral("rawvideo"),
                          QStringLiteral("-")});
    QProcess encoderProcess;
    encoderProcess.setProgram(ffmpegProgram());
    const QString rate = QStringLiteral("%1/%2").arg(source.fpsNum).arg(source.fpsDen);
    const QString part = temporaryName(output, QStringLiteral("mkv"));
    QStringList arguments = {QStringLiteral("-hide_banner"),
                             QStringLiteral("-y"),
                             QStringLiteral("-v"),
                             QStringLiteral("error"),
                             QStringLiteral("-f"),
                             QStringLiteral("rawvideo"),
                             QStringLiteral("-pix_fmt"),
                             QStringLiteral("rgb24"),
                             QStringLiteral("-s"),
                             QStringLiteral("%1x%2").arg(eye.width()).arg(eye.height()),
                             QStringLiteral("-r"),
                             rate,
                             QStringLiteral("-i"),
                             QStringLiteral("-"),
                             QStringLiteral("-i"),
                             source.path,
                             QStringLiteral("-map"),
                             QStringLiteral("0:v")};
    arguments += otherStreams(1);
    arguments += {QStringLiteral("-vf"),
                  QStringLiteral("scale=out_color_matrix=bt709:out_range=tv,format=yuv420p"),
                  QStringLiteral("-colorspace"),
                  QStringLiteral("bt709"),
                  QStringLiteral("-color_primaries"),
                  QStringLiteral("bt709"),
                  QStringLiteral("-color_trc"),
                  QStringLiteral("bt709"),
                  QStringLiteral("-color_range"),
                  QStringLiteral("tv"),
                  QStringLiteral("-c:v"),
                  QStringLiteral("libx264"),
                  QStringLiteral("-crf"),
                  QStringLiteral("18"),
                  QStringLiteral("-preset"),
                  QStringLiteral("medium"),
                  QStringLiteral("-metadata:s:v:0"),
                  QStringLiteral("stereo_mode=anaglyph_cyan_red"),
                  part};
    encoderProcess.setArguments(arguments);
    decoder.start();
    encoderProcess.start();
    if (!decoder.waitForStarted() || !encoderProcess.waitForStarted()) {
        if (error) {
            *error = i18nc("@info %1 is a program", "Cannot start %1.", ffmpegProgram());
        }
        decoder.kill();
        encoderProcess.kill();
        return false;
    }
    const qsizetype in = qsizetype(eye.width()) * 2 * eye.height() * 3;
    const qsizetype outSize = qsizetype(eye.width()) * eye.height() * 3;
    const double frames = source.duration * source.fpsNum / source.fpsDen;
    QByteArray frame;
    QByteArray result(outSize, Qt::Uninitialized);
    qint64 done = 0;
    bool ok = true;
    for (;;) {
        if (progress.cancelled()) {
            ok = false;
            break;
        }
        if (frame.size() < in) {
            if (decoder.bytesAvailable() == 0 && !decoder.waitForReadyRead(200)) {
                if (decoder.state() == QProcess::NotRunning && decoder.bytesAvailable() == 0) {
                    break;
                }
                continue;
            }
            frame += decoder.read(in - frame.size());
            continue;
        }
        const auto *pixels = reinterpret_cast<const uchar *>(frame.constData());
        auto *out = reinterpret_cast<uchar *>(result.data());
        for (int y = 0; y < eye.height(); ++y) {
            const uchar *row = pixels + qsizetype(y) * eye.width() * 6;
            anaglyphRow(row, row + eye.width() * 3, out + qsizetype(y) * eye.width() * 3, eye.width(), AnaglyphPair::RedCyan);
        }
        encoderProcess.write(result);
        while (encoderProcess.bytesToWrite() > 4 * outSize) {
            encoderProcess.waitForBytesWritten(200);
        }
        frame.clear();
        ++done;
        if (progress.report && frames > 0) {
            progress.report(qBound(0.0, done / frames, 1.0));
        }
    }
    encoderProcess.closeWriteChannel();
    if (!ok) {
        decoder.kill();
        encoderProcess.kill();
    }
    decoder.waitForFinished(-1);
    encoderProcess.waitForFinished(-1);
    if (ok && (decoder.exitCode() != 0 || encoderProcess.exitCode() != 0 || done == 0)) {
        if (error) {
            *error = i18nc("@info %1 is an error message", "FFmpeg failed: %1", text(decoder.readAllStandardError() + encoderProcess.readAllStandardError()));
        }
        ok = false;
    }
    if (ok) {
        // RFC 9559 StereoMode 10, anaglyph (cyan/red): the red view is the left eye's
        ok = setStereoMode(part, 10, error) && QFile::rename(part, output);
    }
    if (!ok) {
        QFile::remove(part);
    }
    return ok;
}
}

QString ffmpegProgram()
{
    const QString program = qEnvironmentVariable("STEREO_CREATE_FFMPEG");
    return program.isEmpty() ? QStringLiteral("ffmpeg") : program;
}

QString mkvpropeditProgram()
{
    const QString program = qEnvironmentVariable("STEREO_CREATE_MKVPROPEDIT");
    return program.isEmpty() ? QStringLiteral("mkvpropedit") : program;
}

QString eyesGraph(const Source &source, QSize *eyeSize)
{
    const int width = source.frameSize.width();
    const int height = source.frameSize.height();
    if (width <= 0 || height <= 0) {
        return {};
    }
    const double sar = source.displaySize.width() / double(width);
    const bool right = source.order == Order::RightFirst;
    QString pre;
    QString first;
    QString second;
    QSize eye;
    switch (source.layout) {
    case Layout::SideBySide:
        first = QStringLiteral("crop=iw/2:ih:0:0");
        second = QStringLiteral("crop=iw/2:ih:iw/2:0");
        eye = QSize(even((source.fullViews ? width / 2.0 : width) * sar), height);
        break;
    case Layout::TopAndBottom:
        first = QStringLiteral("crop=iw:ih/2:0:0");
        second = QStringLiteral("crop=iw:ih/2:0:ih/2");
        eye = QSize(even(width * sar), source.fullViews ? even(height / 2.0) : height);
        break;
    case Layout::Rows:
        pre = right ? QStringLiteral("stereo3d=irr:sbsr,") : QStringLiteral("stereo3d=irl:sbsl,");
        first = QStringLiteral("crop=iw/2:ih:0:0");
        second = QStringLiteral("crop=iw/2:ih:iw/2:0");
        eye = QSize(even(width * sar), height);
        break;
    case Layout::Columns:
        pre = right ? QStringLiteral("stereo3d=icr:sbsr,") : QStringLiteral("stereo3d=icl:sbsl,");
        first = QStringLiteral("crop=iw/2:ih:0:0");
        second = QStringLiteral("crop=iw/2:ih:iw/2:0");
        eye = QSize(even(width * sar), height);
        break;
    case Layout::FrameSequential:
        pre = right ? QStringLiteral("stereo3d=ar:sbsr,") : QStringLiteral("stereo3d=al:sbsl,");
        first = QStringLiteral("crop=iw/2:ih:0:0");
        second = QStringLiteral("crop=iw/2:ih:iw/2:0");
        eye = QSize(even(width * sar), height);
        break;
    default:
        return {};
    }
    if (right) {
        std::swap(first, second);
    }
    if (eyeSize) {
        *eyeSize = eye;
    }
    const QString scale = QStringLiteral(",scale=%1:%2:flags=lanczos,setsar=1").arg(eye.width()).arg(eye.height());
    // The views go without the source's Stereo 3D side data: the result gets its own
    return QStringLiteral("[0:v]%1sidedata=mode=delete:type=STEREO3D,split[a][b];[a]%2%3[l];[b]%4%3[r]").arg(pre, first, scale, second);
}

bool convertVideo(const Source &source, Target target, const QString &output, const Progress &progress, QString *error)
{
    QSize eye;
    const QString graph = eyesGraph(source, &eye);
    if (graph.isEmpty()) {
        if (error) {
            *error = i18nc("@info", "This stereo packing cannot be converted yet.");
        }
        return false;
    }
    if (target == Target::VideoAnaglyph) {
        if (source.colorTransfer == QLatin1String("smpte2084") || source.colorTransfer == QLatin1String("arib-std-b67")) {
            if (error) {
                *error = i18nc("@info", "An anaglyph needs a video in standard dynamic range; this one is HDR.");
            }
            return false;
        }
        return convertAnaglyph(source, graph, eye, output, progress, error);
    }
    QString stack;
    QString seiType;
    int stereoMode = 0;
    QString modeName;
    switch (target) {
    case Target::VideoFullSideBySide:
    case Target::VideoMp4:
        stack = QStringLiteral(";[l][r]hstack%1[v]").arg(colourDescription(source));
        seiType = QStringLiteral("side_by_side");
        stereoMode = 1;
        modeName = QStringLiteral("left_right");
        break;
    case Target::VideoHalfSideBySide:
        stack = QStringLiteral(";[l]scale=%1:%2[l2];[r]scale=%1:%2[r2];[l2][r2]hstack%3[v]")
                    .arg(even(eye.width() / 2.0))
                    .arg(eye.height())
                    .arg(colourDescription(source));
        seiType = QStringLiteral("side_by_side");
        stereoMode = 1;
        modeName = QStringLiteral("left_right");
        break;
    case Target::VideoTopBottom:
        stack = QStringLiteral(";[l]scale=%1:%2[l2];[r]scale=%1:%2[r2];[l2][r2]vstack%3[v]")
                    .arg(eye.width())
                    .arg(even(eye.height() / 2.0))
                    .arg(colourDescription(source));
        seiType = QStringLiteral("top_bottom");
        stereoMode = 3;
        modeName = QStringLiteral("top_bottom");
        break;
    default:
        return false;
    }
    // Matroska first: the picture encoded with its SEI, the other streams as they are, then the StereoMode element
    const QString mkv = temporaryName(output, QStringLiteral("mkv"));
    QStringList arguments =
        {QStringLiteral("-i"), source.path, QStringLiteral("-filter_complex"), graph + stack, QStringLiteral("-map"), QStringLiteral("[v]")};
    arguments += otherStreams(0);
    arguments += encoder(source);
    arguments += colourOptions(source);
    arguments += framePackingSei(source, seiType);
    arguments += {QStringLiteral("-metadata:s:v:0"), QStringLiteral("stereo_mode=") + modeName, mkv};
    const Progress encodeProgress{[&](double f) {
                                      if (progress.report) {
                                          progress.report(target == Target::VideoMp4 ? f * 0.95 : f);
                                      }
                                  },
                                  progress.cancel};
    bool ok = runFfmpeg(arguments, source.duration, encodeProgress, error) && setStereoMode(mkv, stereoMode, error);
    if (ok && target == Target::VideoMp4) {
        // MP4 from the marked Matroska file: the same stream with its SEI, and the st3d box the MP4 muxer writes from
        // the StereoMode it reads (Spherical Video V2, an "unofficial" box for FFmpeg); audio MP4 cannot carry is
        // encoded to AAC, subtitles stay in the Matroska targets
        static const QStringList mp4Audio = {QStringLiteral("aac"),
                                             QStringLiteral("mp3"),
                                             QStringLiteral("ac3"),
                                             QStringLiteral("eac3"),
                                             QStringLiteral("alac"),
                                             QStringLiteral("opus"),
                                             QStringLiteral("flac")};
        bool copyAudio = true;
        for (const QString &codec : source.audioCodecs) {
            copyAudio &= mp4Audio.contains(codec);
        }
        const QString part = temporaryName(output, QStringLiteral("mp4"));
        ok = runFfmpeg({QStringLiteral("-i"),
                        mkv,
                        QStringLiteral("-map"),
                        QStringLiteral("0:v"),
                        QStringLiteral("-map"),
                        QStringLiteral("0:a?"),
                        QStringLiteral("-c:v"),
                        QStringLiteral("copy"),
                        QStringLiteral("-c:a"),
                        copyAudio ? QStringLiteral("copy") : QStringLiteral("aac"),
                        QStringLiteral("-strict"),
                        QStringLiteral("unofficial"),
                        QStringLiteral("-f"),
                        QStringLiteral("mp4"),
                        part},
                       source.duration,
                       Progress{nullptr, progress.cancel},
                       error)
            && removeVexu(part, error) && QFile::rename(part, output);
        if (!ok) {
            QFile::remove(part);
        }
        QFile::remove(mkv);
    } else if (ok) {
        ok = QFile::rename(mkv, output);
    }
    if (!ok) {
        QFile::remove(mkv);
    }
    if (ok && progress.report) {
        progress.report(1.0);
    }
    return ok;
}

bool grabFrame(const Source &source, double time, StereoPair *pair, const Progress &progress, QString *error)
{
    QSize eye;
    const QString graph = eyesGraph(source, &eye);
    if (graph.isEmpty()) {
        if (error) {
            *error = i18nc("@info", "This stereo packing cannot be converted yet.");
        }
        return false;
    }
    QTemporaryDir dir;
    const QString left = dir.filePath(QStringLiteral("left.png"));
    const QString right = dir.filePath(QStringLiteral("right.png"));
    if (!runFfmpeg({QStringLiteral("-ss"),
                    QString::number(time, 'f', 3),
                    QStringLiteral("-i"),
                    source.path,
                    QStringLiteral("-filter_complex"),
                    graph,
                    QStringLiteral("-map"),
                    QStringLiteral("[l]"),
                    QStringLiteral("-frames:v"),
                    QStringLiteral("1"),
                    left,
                    QStringLiteral("-map"),
                    QStringLiteral("[r]"),
                    QStringLiteral("-frames:v"),
                    QStringLiteral("1"),
                    right},
                   0,
                   progress,
                   error)) {
        return false;
    }
    pair->left = QImage(left);
    pair->right = QImage(right);
    if (!pair->isValid()) {
        if (error) {
            *error = i18nc("@info", "No frame could be read at that time.");
        }
        return false;
    }
    return true;
}
}
