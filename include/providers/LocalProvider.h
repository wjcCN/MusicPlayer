#pragma once

#include "core/MusicTrack.h"
#include "providers/MusicProvider.h"

#include <QList>
#include <QStringList>

class LocalProvider final : public MusicProvider
{
    Q_OBJECT

public:
    explicit LocalProvider(QObject *parent = nullptr);

    QString id() const override;
    QString displayName() const override;
    ProviderInfo info() const override;

    QList<MusicTrack> scanFolder(const QString &folderPath) const;
    static bool isSupportedFile(const QString &filePath);
    static QStringList supportedSuffixes();
};
