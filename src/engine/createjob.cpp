// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Daniel Campos Ramos <Capitain_Jack@yahoo.com>
#include "createjob.h"

#include "convertvideo.h"
#include "photo.h"
#include "probe.h"

#include <KLocalizedString>

#include <QFile>
#include <QFileInfo>
#include <QUrl>
#include <QtConcurrentRun>

namespace StereoCreate
{
namespace
{
QString fileName(const QString &path)
{
    return QFileInfo(path).fileName();
}

bool writePhoto(Target target, const StereoPair &pair, const QString &source, int quality, bool lossless, QStringList *outputs, QString *error)
{
    const QString anaglyphExtension = lossless ? QStringLiteral("png") : QStringLiteral("jpg");
    auto anaglyphFile = [&](AnaglyphPair colours, const QString &suffix) {
        const QString path = outputPath(source, suffix, anaglyphExtension);
        if (!writeAnaglyph(pair, colours, path, quality, error)) {
            return false;
        }
        outputs->append(path);
        return true;
    };
    switch (target) {
    case Target::Jps:
    case Target::FrameJps: {
        const QString path = outputPath(source, target == Target::FrameJps ? QStringLiteral("-frame") : QString(), QStringLiteral("jps"));
        if (!writeJps(pair, path, quality, error)) {
            return false;
        }
        outputs->append(path);
        return true;
    }
    case Target::Mpo:
    case Target::FrameMpo: {
        const QString path = outputPath(source, target == Target::FrameMpo ? QStringLiteral("-frame") : QString(), QStringLiteral("mpo"));
        if (!writeMpo(pair, path, quality, error)) {
            return false;
        }
        outputs->append(path);
        return true;
    }
    case Target::AnaglyphRedCyan:
        return anaglyphFile(AnaglyphPair::RedCyan, QStringLiteral("-anaglyph-red-cyan"));
    case Target::FrameAnaglyph:
        return anaglyphFile(AnaglyphPair::RedCyan, QStringLiteral("-frame-anaglyph-red-cyan"));
    case Target::AnaglyphRedCyanCrt:
        return anaglyphFile(AnaglyphPair::RedCyanCrt, QStringLiteral("-anaglyph-red-cyan-crt"));
    case Target::AnaglyphGreenMagenta:
        return anaglyphFile(AnaglyphPair::GreenMagenta, QStringLiteral("-anaglyph-green-magenta"));
    case Target::AnaglyphAmberBlue:
        return anaglyphFile(AnaglyphPair::AmberBlue, QStringLiteral("-anaglyph-amber-blue"));
    case Target::SideBySidePng: {
        const QString path = outputPath(source, QStringLiteral("-sbs"), QStringLiteral("png"));
        if (!writeSideBySidePng(pair, path, true, error)) {
            return false;
        }
        outputs->append(path);
        return true;
    }
    case Target::Pns: {
        const QString path = outputPath(source, QString(), QStringLiteral("pns"));
        if (!writeSideBySidePng(pair, path, false, error)) {
            return false;
        }
        outputs->append(path);
        return true;
    }
    case Target::ProofPair: {
        const QString mpo = outputPath(source, QString(), QStringLiteral("mpo"));
        if (!writeMpo(pair, mpo, quality, error)) {
            return false;
        }
        outputs->append(mpo);
        return anaglyphFile(AnaglyphPair::RedCyan, QStringLiteral("-anaglyph-red-cyan"));
    }
    default:
        return false;
    }
}
}

CreateJob::CreateJob(const QStringList &files, Target target, QObject *parent)
    : KJob(parent)
    , m_files(files)
    , m_target(target)
    , m_quality(DefaultQuality)
{
    setCapabilities(KJob::Killable);
    // Started by the user from a menu: shown in the notification area at once, also when it ends soon
    setProperty("immediateProgressReporting", true);
    for (const QString &file : files) {
        const qint64 size = qMax<qint64>(1, QFileInfo(file).size());
        m_sizes.append(size);
        m_totalBytes += size;
    }
    if (!files.isEmpty()) {
        // The finished job offers the folder of the new files
        setProperty("destUrl", QUrl::fromLocalFile(QFileInfo(files.constFirst()).absolutePath()));
    }
}

CreateJob::~CreateJob()
{
    m_cancel = true;
    m_watcher.waitForFinished();
}

void CreateJob::setQuality(int quality)
{
    m_quality = quality;
}

void CreateJob::setFrameTime(double seconds)
{
    m_frameTime = seconds;
}

void CreateJob::setLosslessAnaglyph(bool lossless)
{
    m_lossless = lossless;
}

QStringList CreateJob::outputs() const
{
    return m_outputs;
}

void CreateJob::start()
{
    m_timer.start();
    setTotalAmount(KJob::Files, m_files.size());
    setTotalAmount(KJob::Bytes, m_totalBytes);
    connect(&m_watcher, &QFutureWatcher<void>::finished, this, [this] {
        if (error() == KJob::KilledJobError || m_cancel) {
            return;
        }
        if (!m_errors.isEmpty()) {
            setError(KJob::UserDefinedError);
            setErrorText(m_errors.join(QLatin1Char('\n')));
        }
        emitResult();
    });
    m_watcher.setFuture(QtConcurrent::run([this] {
        work();
    }));
}

bool CreateJob::doKill()
{
    m_cancel = true;
    m_watcher.waitForFinished();
    // what a cancelled run made stays only when it was finished: the file in progress is removed by its step
    return true;
}

void CreateJob::report(int index, double fraction)
{
    qint64 done = 0;
    for (int i = 0; i < index; ++i) {
        done += m_sizes.at(i);
    }
    done += qint64(m_sizes.at(index) * fraction);
    // The bytes of the sources done per second, so that the notification can tell the time left
    const qint64 elapsed = qMax<qint64>(1, m_timer.elapsed());
    QMetaObject::invokeMethod(this, [this, index, done, elapsed] {
        setProcessedAmount(KJob::Files, index);
        setProcessedAmount(KJob::Bytes, done);
        setPercent(qMin<qulonglong>(99, done * 100 / qMax<qint64>(1, m_totalBytes)));
        emitSpeed(done * 1000 / elapsed);
    });
}

void CreateJob::work()
{
    for (int index = 0; index < m_files.size() && !m_cancel; ++index) {
        const QString file = m_files.at(index);
        QMetaObject::invokeMethod(this, [this, file] {
            Q_EMIT description(this,
                               i18nc("@title job, %1 is a kind of file such as MPO", "Creating %1", targetText(m_target)),
                               qMakePair(i18nc("@label the file a stereo file is made from", "Source"), fileName(file)));
        });
        report(index, 0);
        QStringList outputs;
        QString error;
        if (process(index, &outputs, &error)) {
            m_outputs += outputs;
        } else if (!m_cancel) {
            m_errors.append(i18nc("@info %1 is a file name, %2 an error message", "%1: %2", fileName(file), error));
        }
        report(index, 1.0);
        QMetaObject::invokeMethod(this, [this, index] {
            setProcessedAmount(KJob::Files, index + 1);
        });
    }
}

bool CreateJob::process(int index, QStringList *outputs, QString *error)
{
    const QString file = m_files.at(index);
    const Source source = probe(file);
    if (!source.stereo) {
        *error = i18nc("@info", "Not a stereo picture or video.");
        return false;
    }
    const TargetInfo &info = targetInfo(m_target);
    const Progress progress{[this, index](double fraction) {
                                report(index, fraction);
                            },
                            &m_cancel};
    if (info.kind == SourceKind::Picture) {
        if (source.kind != SourceKind::Picture) {
            *error = i18nc("@info", "This is made from a picture, not a video.");
            return false;
        }
        const Stereo::Picture picture = Stereo::Picture::load(file, {}, Stereo::OutputMode::Stereo);
        const StereoPair pair{picture.left, picture.right};
        if (!pair.isValid()) {
            *error = i18nc("@info", "The views of the picture cannot be read.");
            return false;
        }
        return writePhoto(m_target, pair, file, m_quality, m_lossless, outputs, error);
    }
    if (source.kind != SourceKind::Video) {
        *error = i18nc("@info", "This is made from a video, not a picture.");
        return false;
    }
    switch (m_target) {
    case Target::FrameMpo:
    case Target::FrameJps:
    case Target::FrameAnaglyph: {
        StereoPair pair;
        const double time = m_frameTime >= 0 ? m_frameTime : source.duration / 3;
        return grabFrame(source, time, &pair, progress, error) && writePhoto(m_target, pair, file, m_quality, m_lossless, outputs, error);
    }
    default: {
        static const QHash<Target, QPair<QString, QString>> names = {
            {Target::VideoFullSideBySide, {QStringLiteral("-full-sbs"), QStringLiteral("mkv")}},
            {Target::VideoHalfSideBySide, {QStringLiteral("-half-sbs"), QStringLiteral("mkv")}},
            {Target::VideoTopBottom, {QStringLiteral("-top-bottom"), QStringLiteral("mkv")}},
            {Target::VideoMp4, {QStringLiteral("-sbs"), QStringLiteral("mp4")}},
            {Target::VideoAnaglyph, {QStringLiteral("-anaglyph-red-cyan"), QStringLiteral("mkv")}},
        };
        const auto name = names.value(m_target);
        const QString output = outputPath(file, name.first, name.second);
        if (!convertVideo(source, m_target, output, progress, error)) {
            return false;
        }
        outputs->append(output);
        return true;
    }
    }
}
}

#include "moc_createjob.cpp"
