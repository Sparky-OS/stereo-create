// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Daniel Campos Ramos <Capitain_Jack@yahoo.com>
#pragma once

#include "targets.h"

#include <KJob>

#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QStringList>

#include <atomic>

namespace StereoCreate
{
// One run of "Create stereo": every selected file into one target, as one job (progress, time left and cancel in the
// notification area through the job tracker). New files go next to their sources; a file that fails is reported and
// the others go on.
class CreateJob : public KJob
{
    Q_OBJECT

public:
    CreateJob(const QStringList &files, Target target, QObject *parent = nullptr);
    ~CreateJob() override;

    void setQuality(int quality);
    // The time of a frame grab in seconds; below zero (the default) a third of the video
    void setFrameTime(double seconds);
    // Anaglyphs as PNG instead of JPEG
    void setLosslessAnaglyph(bool lossless);

    void start() override;
    QStringList outputs() const;

protected:
    bool doKill() override;

private:
    void work();
    bool process(int index, QStringList *outputs, QString *error);
    void report(int index, double fraction);

    const QStringList m_files;
    const Target m_target;
    int m_quality;
    double m_frameTime = -1;
    bool m_lossless = false;
    qint64 m_totalBytes = 0;
    QList<qint64> m_sizes;
    QStringList m_outputs;
    QStringList m_errors;
    std::atomic<bool> m_cancel = false;
    QElapsedTimer m_timer;
    QFutureWatcher<void> m_watcher;
};
}
