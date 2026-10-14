// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Daniel Campos Ramos <Capitain_Jack@yahoo.com>
#include "stereocreateitemaction.h"

#include "probe.h"
#include "targets.h"

#include <KFileItem>
#include <KFileItemListProperties>
#include <KLocalizedString>
#include <KPluginFactory>

#include <QAction>
#include <QIcon>
#include <QMenu>
#include <QProcess>
#include <QStandardPaths>

#include <optional>

using namespace StereoCreate;

K_PLUGIN_CLASS_WITH_JSON(StereoCreateItemAction, "stereocreateitemaction.json")

namespace
{
// A context menu looks at no more files than this; a larger selection is offered when these are all stereo
constexpr int MaxProbed = 32;
}

StereoCreateItemAction::StereoCreateItemAction(QObject *parent, const QVariantList &)
    : KAbstractFileItemActionPlugin(parent)
{
}

QList<QAction *> StereoCreateItemAction::actions(const KFileItemListProperties &fileItemInfos, QWidget *parentWidget)
{
    const KFileItemList items = fileItemInfos.items();
    if (items.isEmpty() || !fileItemInfos.isLocal()) {
        return {};
    }
    QStringList files;
    std::optional<SourceKind> kind;
    for (const KFileItem &item : items) {
        const QString path = item.localPath();
        if (path.isEmpty() || item.isDir()) {
            return {};
        }
        files.append(path);
        if (files.size() > MaxProbed) {
            continue;
        }
        const Source source = probe(path, true);
        if (!source.stereo || (kind && *kind != source.kind)) {
            return {};
        }
        kind = source.kind;
    }

    auto *menu = new QMenu(parentWidget);
    auto add = [&](Target target) {
        QAction *action = menu->addAction(targetText(target));
        const QString id = QString::fromLatin1(targetInfo(target).id);
        connect(action, &QAction::triggered, this, [this, id, files] {
            const QString program = QStandardPaths::findExecutable(QStringLiteral("stereo-create"));
            if (program.isEmpty() || !QProcess::startDetached(program, QStringList{QStringLiteral("--to"), id} + files)) {
                Q_EMIT error(i18nc("@info", "The stereo-create program cannot be started."));
            }
        });
    };
    if (*kind == SourceKind::Picture) {
        add(Target::ProofPair);
        menu->addSeparator();
        for (const Target target : {Target::Mpo, Target::Jps, Target::SideBySidePng, Target::Pns}) {
            add(target);
        }
        menu->addSeparator();
        for (const Target target : {Target::AnaglyphRedCyan, Target::AnaglyphRedCyanCrt, Target::AnaglyphGreenMagenta, Target::AnaglyphAmberBlue}) {
            add(target);
        }
    } else {
        for (const Target target :
             {Target::VideoFullSideBySide, Target::VideoHalfSideBySide, Target::VideoTopBottom, Target::VideoMp4, Target::VideoAnaglyph}) {
            add(target);
        }
        menu->addSeparator();
        for (const Target target : {Target::FrameMpo, Target::FrameJps, Target::FrameAnaglyph}) {
            add(target);
        }
    }
    auto *action = new QAction(QIcon::fromTheme(QStringLiteral("view-stereo"), QIcon::fromTheme(QStringLiteral("document-new"))),
                               i18nc("@action:inmenu", "Create Stereo"),
                               parentWidget);
    action->setMenu(menu);
    connect(action, &QObject::destroyed, menu, &QObject::deleteLater);
    return {action};
}

#include "stereocreateitemaction.moc"
