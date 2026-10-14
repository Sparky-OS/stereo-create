// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Daniel Campos Ramos <Capitain_Jack@yahoo.com>
#include "createjob.h"
#include "probe.h"
#include "targets.h"

#include "stereo_create_version.h"

#include <KAboutData>
#include <KLocalizedString>
#include <KUiServerV2JobTracker>

#include <QApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QTextStream>
#include <QTimer>

#include <cstdio>

using namespace StereoCreate;

int main(int argc, char **argv)
{
    // A script or a terminal without a desktop still works, without the notification
    if (qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY") && qEnvironmentVariableIsEmpty("DISPLAY") && qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }
    QApplication app(argc, argv);
    KLocalizedString::setApplicationDomain("stereo-create");
    KAboutData about(QStringLiteral("stereo-create"),
                     i18nc("@title", "Create Stereo"),
                     QStringLiteral(STEREO_CREATE_VERSION_STRING),
                     i18nc("@info", "Makes stereo pictures and videos in other stereo formats."),
                     KAboutLicense::GPL_V2,
                     i18nc("@info", "Sparky Stereo OS, Daniel Ramos's edition of SparkyLinux (by Paweł \"pavroo\" Pijanowski)"));
    about.addAuthor(QStringLiteral("Daniel Campos Ramos"), QString(), QStringLiteral("Capitain_Jack@yahoo.com"));
    about.setDesktopFileName(QStringLiteral("org.sparky.stereo-create"));
    KAboutData::setApplicationData(about);

    QCommandLineParser parser;
    about.setupCommandLine(&parser);
    QStringList ids;
    for (const TargetInfo &info : targets()) {
        ids.append(QString::fromLatin1(info.id));
    }
    const QCommandLineOption to(QStringLiteral("to"),
                                i18nc("@info:shell %1 is a list of names", "What to make: one of %1.", ids.join(QStringLiteral(", "))),
                                QStringLiteral("target"));
    const QCommandLineOption quality(QStringLiteral("quality"), i18nc("@info:shell", "JPEG quality, 1 to 100 (default 92)."), QStringLiteral("quality"));
    const QCommandLineOption time(QStringLiteral("time"),
                                  i18nc("@info:shell", "The time of a frame grab in seconds (default: a third of the video)."),
                                  QStringLiteral("seconds"));
    const QCommandLineOption lossless(QStringLiteral("lossless-anaglyph"), i18nc("@info:shell", "Write anaglyphs as PNG instead of JPEG."));
    const QCommandLineOption probeOption(QStringLiteral("probe"), i18nc("@info:shell", "Only tell whether each file is stereo, and how."));
    const QCommandLineOption quiet(QStringLiteral("no-notification"), i18nc("@info:shell", "Do not show the progress in the notification area."));
    parser.addOptions({to, quality, time, lossless, probeOption, quiet});
    parser.addPositionalArgument(QStringLiteral("files"), i18nc("@info:shell", "Stereo pictures or videos."), QStringLiteral("files..."));
    parser.process(app);
    about.processCommandLine(&parser);

    QStringList files;
    for (const QString &argument : parser.positionalArguments()) {
        files.append(QFileInfo(argument).absoluteFilePath());
    }
    if (files.isEmpty()) {
        parser.showHelp(1);
    }
    QTextStream out(stdout);
    if (parser.isSet(probeOption)) {
        static const char *layouts[] =
            {"none", "sideBySide", "topAndBottom", "multiView", "rows", "columns", "checkerboard", "sideBySideQuincunx", "frameSequential", "blockLaced"};
        static const char *origins[] = {"none", "mark", "user", "format", "detection", "shape"};
        for (const QString &file : std::as_const(files)) {
            const Source source = probe(file);
            out << file << '\t' << (source.stereo ? "stereo" : "flat") << '\t' << layouts[int(source.layout)] << '\t'
                << (source.order == Stereo::Picture::EyeOrder::LeftFirst ? "leftFirst" : "rightFirst") << '\t' << origins[int(source.origin)];
            if (source.kind == SourceKind::Video) {
                out << '\t' << (source.fullViews ? "full" : "half") << '\t' << (source.mark.isEmpty() ? QStringLiteral("-") : source.mark);
            }
            out << '\n';
        }
        return 0;
    }
    const TargetInfo *target = targetById(parser.value(to));
    if (!target) {
        fprintf(stderr, "%s\n", qPrintable(i18nc("@info:shell %1 is a list of names", "Choose what to make with --to: %1.", ids.join(QStringLiteral(", ")))));
        return 1;
    }
    auto *job = new CreateJob(files, target->target);
    if (parser.isSet(quality)) {
        job->setQuality(qBound(1, parser.value(quality).toInt(), 100));
    }
    if (parser.isSet(time)) {
        job->setFrameTime(parser.value(time).toDouble());
    }
    job->setLosslessAnaglyph(parser.isSet(lossless));
    if (!parser.isSet(quiet)) {
        auto *tracker = new KUiServerV2JobTracker(&app);
        tracker->registerJob(job);
    }
    QObject::connect(job, &KJob::percentChanged, &app, [](KJob *, unsigned long percent) {
        fprintf(stderr, "%lu%%\n", percent);
    });
    int status = 0;
    QObject::connect(job, &KJob::result, &app, [&](KJob *) {
        for (const QString &output : job->outputs()) {
            out << output << '\n';
        }
        out.flush();
        if (job->error()) {
            fprintf(stderr, "%s\n", qPrintable(job->errorString()));
            status = job->error() == KJob::KilledJobError ? 3 : 2;
        }
        // a moment for the tracker to tell the notification area the job ended
        QTimer::singleShot(500, &app, &QCoreApplication::quit);
    });
    job->start();
    app.exec();
    return status;
}
