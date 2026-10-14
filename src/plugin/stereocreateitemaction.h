// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Daniel Campos Ramos <Capitain_Jack@yahoo.com>
#pragma once

#include <KAbstractFileItemActionPlugin>

// "Create Stereo" in the context menu of stereo pictures and videos only: what the shared stereo policy takes for a
// pair (a mark, a stereo format, the shape rule), with the user's swap-eyes correction. Each entry runs stereo-create.
class StereoCreateItemAction : public KAbstractFileItemActionPlugin
{
    Q_OBJECT

public:
    StereoCreateItemAction(QObject *parent, const QVariantList &args);
    QList<QAction *> actions(const KFileItemListProperties &fileItemInfos, QWidget *parentWidget) override;
};
