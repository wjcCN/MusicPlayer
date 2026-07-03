#pragma once

#include "core/MusicTrack.h"
#include "providers/LocalProvider.h"

#include <QObject>
#include <QSqlDatabase>
#include <QStringList>

class QSqlQuery;

class MusicLibrary final : public QObject
{
    Q_OBJECT

public:
    explicit MusicLibrary(QObject *parent = nullptr);
    ~MusicLibrary() override;

    bool open();
    QString lastError() const;

    QList<MusicTrack> tracks() const;
    QStringList folders() const;
    int addFolder(const QString &folderPath);
    int rescanFolders();
    bool setTrackDuration(const QString &filePath, qint64 durationMs);
    bool setFavorite(const QString &filePath, bool favorite);
    bool markPlayed(const QString &filePath);

    QList<MusicTrack> loadQueue() const;
    bool saveQueue(const QList<MusicTrack> &queue) const;

    QString setting(const QString &key, const QString &fallback = QString()) const;
    bool setSetting(const QString &key, const QString &value) const;

signals:
    void libraryChanged();
    void statusMessage(const QString &message);

private:
    bool ensureSchema();
    bool upsertTrack(const MusicTrack &track) const;
    MusicTrack trackFromQuery(const QSqlQuery &query) const;
    QList<MusicTrack> scanFolder(const QString &folderPath) const;

    QSqlDatabase m_db;
    QString m_lastError;
    LocalProvider m_localProvider;
};
