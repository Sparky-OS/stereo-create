// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Daniel Campos Ramos <Capitain_Jack@yahoo.com>
#include "probe.h"

#include <video.h>

#include <QFile>
#include <QMimeDatabase>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
#include <libavutil/stereo3d.h>
}

namespace StereoCreate
{
namespace
{
QString mimeName(const QString &path)
{
    static const QMimeDatabase db;
    return db.mimeTypeForFile(path).name();
}

QString markOf(const AVStereo3D *stereo)
{
    return stereo ? markForStereo3D(stereo->type, stereo->flags & AV_STEREO3D_FLAG_INVERT) : QString();
}

// The frame packing SEI arrives with the decoded frame: the first one is decoded for it
QString firstFrameMark(AVFormatContext *format, int streamIndex)
{
    const AVStream *stream = format->streams[streamIndex];
    const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec) {
        return {};
    }
    AVCodecContext *context = avcodec_alloc_context3(codec);
    QString mark;
    if (context && avcodec_parameters_to_context(context, stream->codecpar) >= 0 && avcodec_open2(context, codec, nullptr) >= 0) {
        AVPacket *packet = av_packet_alloc();
        AVFrame *frame = av_frame_alloc();
        bool done = false;
        for (int packets = 0; !done && packets < 200 && av_read_frame(format, packet) >= 0; ++packets) {
            if (packet->stream_index == streamIndex && avcodec_send_packet(context, packet) >= 0) {
                if (avcodec_receive_frame(context, frame) >= 0) {
                    if (const AVFrameSideData *data = av_frame_get_side_data(frame, AV_FRAME_DATA_STEREO3D)) {
                        mark = markOf(reinterpret_cast<const AVStereo3D *>(data->data));
                    }
                    done = true;
                }
            }
            av_packet_unref(packet);
        }
        av_frame_free(&frame);
        av_packet_free(&packet);
    }
    avcodec_free_context(&context);
    return mark;
}
}

QString markForStereo3D(int type, bool inverted)
{
    const QString order = inverted ? QStringLiteral("RightFirst") : QStringLiteral("LeftFirst");
    switch (type) {
    case AV_STEREO3D_2D:
        return QStringLiteral("mono");
    case AV_STEREO3D_SIDEBYSIDE:
        return QStringLiteral("sideBySide") + order;
    case AV_STEREO3D_TOPBOTTOM:
        return QStringLiteral("topBottom") + order;
    case AV_STEREO3D_FRAMESEQUENCE:
        return QStringLiteral("frameSequential") + order;
    case AV_STEREO3D_CHECKERBOARD:
        return QStringLiteral("checkerboard") + order;
    case AV_STEREO3D_SIDEBYSIDE_QUINCUNX:
        return QStringLiteral("sideBySideQuincunx") + order;
    case AV_STEREO3D_LINES:
        return QStringLiteral("rowInterleaved") + order;
    case AV_STEREO3D_COLUMNS:
        return QStringLiteral("columnInterleaved") + order;
    default:
        return {};
    }
}

bool isVideo(const QString &path)
{
    return mimeName(path).startsWith(QLatin1String("video/"));
}

bool isPicture(const QString &path)
{
    return mimeName(path).startsWith(QLatin1String("image/"));
}

Source probe(const QString &path, bool quick)
{
    Source source;
    source.path = path;
    Stereo::Picture::Options options;
    if (isVideo(path)) {
        source.kind = SourceKind::Video;
        AVFormatContext *format = nullptr;
        if (avformat_open_input(&format, QFile::encodeName(path).constData(), nullptr, nullptr) < 0) {
            return source;
        }
        if (avformat_find_stream_info(format, nullptr) >= 0) {
            const int index = av_find_best_stream(format, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
            if (index >= 0) {
                AVStream *stream = format->streams[index];
                const AVCodecParameters *par = stream->codecpar;
                source.frameSize = QSize(par->width, par->height);
                const AVRational sar = av_guess_sample_aspect_ratio(format, stream, nullptr);
                const double ratio = sar.num > 0 && sar.den > 0 ? av_q2d(sar) : 1.0;
                source.displaySize = QSize(qRound(par->width * ratio), par->height);
                source.duration = format->duration > 0 ? format->duration / double(AV_TIME_BASE) : 0;
                source.codec = QString::fromLatin1(avcodec_get_name(par->codec_id));
                if (const AVPixFmtDescriptor *pixel = av_pix_fmt_desc_get(AVPixelFormat(par->format))) {
                    source.bitDepth = pixel->comp[0].depth;
                }
                auto known = [](const char *name) {
                    const QString value = QString::fromLatin1(name);
                    return value == QLatin1String("unknown") || value == QLatin1String("unspecified") ? QString() : value;
                };
                source.colorPrimaries = known(av_color_primaries_name(par->color_primaries));
                source.colorTransfer = known(av_color_transfer_name(par->color_trc));
                source.colorSpace = known(av_color_space_name(par->color_space));
                source.colorRange = known(av_color_range_name(par->color_range));
                if (stream->avg_frame_rate.num > 0 && stream->avg_frame_rate.den > 0) {
                    source.fpsNum = stream->avg_frame_rate.num;
                    source.fpsDen = stream->avg_frame_rate.den;
                }
                for (unsigned i = 0; i < format->nb_streams; ++i) {
                    if (format->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                        source.audioCodecs.append(QString::fromLatin1(avcodec_get_name(format->streams[i]->codecpar->codec_id)));
                    }
                }
                if (const AVPacketSideData *data = av_packet_side_data_get(par->coded_side_data, par->nb_coded_side_data, AV_PKT_DATA_STEREO3D)) {
                    source.mark = markOf(reinterpret_cast<const AVStereo3D *>(data->data));
                }
                if (source.mark.isEmpty()) {
                    source.mark = firstFrameMark(format, index);
                }
            }
        }
        avformat_close_input(&format);
        if (source.displaySize.isEmpty()) {
            return source;
        }
        options.swapEyes = Stereo::Picture::userSwapEyes(path);
        const Stereo::Video::Layout layout = Stereo::Video::layout(source.displaySize, source.mark, options);
        source.layout = layout.layout;
        source.order = layout.eyeOrder;
        source.origin = layout.source;
        source.stereo = layout.layout != Stereo::Picture::Layout::None;
        source.fullViews = source.stereo && Stereo::Video::fullViews(source.displaySize, layout.layout);
        return source;
    }
    // A picture: the policy of the shared layer, the user's correction included
    source.kind = SourceKind::Picture;
    const Stereo::Picture pair =
        quick ? Stereo::Picture::loadPair(path, QSize(64, 64), options) : Stereo::Picture::load(path, options, Stereo::OutputMode::Stereo);
    source.stereo = pair.isStereo();
    source.layout = pair.layout;
    source.order = pair.eyeOrder;
    source.origin = pair.source;
    source.frameSize = pair.original.size();
    return source;
}
}
