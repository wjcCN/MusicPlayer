#include "data/MusicLibrary.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>
#include <QVariant>

MusicLibrary::MusicLibrary(QObject *parent)
    : QObject(parent)
{
}

MusicLibrary::~MusicLibrary()
{
    const QString connectionName = m_db.connectionName();
    if (m_db.isOpen()) {
        m_db.close();
    }
    if (!connectionName.isEmpty()) {
        m_db = QSqlDatabase();
        QSqlDatabase::removeDatabase(connectionName);
    }
}

bool MusicLibrary::open()
{
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dataDir.isEmpty()) {
        m_lastError = tr("Could not resolve an application data directory.");
        return false;
    }

    QDir dir;
    if (!dir.mkpath(dataDir)) {
        m_lastError = tr("Could not create application data directory: %1").arg(dataDir);
        return false;
    }

    const QString connectionName = QStringLiteral("music-library-%1").arg(QUuid::createUuid().toString(QUuid::Id128));
    m_db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    m_db.setDatabaseName(dataDir + "/library.sqlite");

    if (!m_db.open()) {
        m_lastError = m_db.lastError().text();
        return false;
    }

    return ensureSchema();
}

QString MusicLibrary::lastError() const
{
    return m_lastError;
}

QList<MusicTrack> MusicLibrary::tracks() const
{
    QList<MusicTrack> result;
    QSqlQuery query(m_db);
    query.prepare("SELECT id, provider_id, title, artist, album, file_path, duration_ms, favorite, added_at "
                  "FROM tracks ORDER BY title COLLATE NOCASE, file_path COLLATE NOCASE");

    if (!query.exec()) {
        return result;
    }

    while (query.next()) {
        result.append(trackFromQuery(query));
    }

    return result;
}

QStringList MusicLibrary::folders() const
{
    QStringList result;
    QSqlQuery query(m_db);
    query.prepare("SELECT path FROM folders ORDER BY path COLLATE NOCASE");

    if (!query.exec()) {
        return result;
    }

    while (query.next()) {
        result.append(query.value(0).toString());
    }

    return result;
}

int MusicLibrary::addFolder(const QString &folderPath)
{
    const QFileInfo folderInfo(folderPath);
    if (!folderInfo.exists() || !folderInfo.isDir()) {
        emit statusMessage(tr("Folder does not exist: %1").arg(folderPath));
        return 0;
    }

    const QString normalizedPath = QDir::toNativeSeparators(folderInfo.absoluteFilePath());
    QSqlQuery folderQuery(m_db);
    folderQuery.prepare("INSERT OR IGNORE INTO folders(path, added_at) VALUES(?, ?)");
    folderQuery.addBindValue(normalizedPath);
    folderQuery.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    folderQuery.exec();

    int changed = 0;
    const QList<MusicTrack> scannedTracks = scanFolder(normalizedPath);

    m_db.transaction();
    for (const MusicTrack &track : scannedTracks) {
        if (upsertTrack(track)) {
            ++changed;
        }
    }
    m_db.commit();

    emit libraryChanged();
    emit statusMessage(tr("Scanned %1 supported audio file(s).").arg(scannedTracks.size()));
    return changed;
}

int MusicLibrary::rescanFolders()
{
    int changed = 0;
    const QStringList knownFolders = folders();

    for (const QString &folder : knownFolders) {
        changed += addFolder(folder);
    }

    return changed;
}

bool MusicLibrary::setTrackDuration(const QString &filePath, qint64 durationMs)
{
    if (filePath.isEmpty() || durationMs <= 0) {
        return false;
    }

    QSqlQuery query(m_db);
    query.prepare("UPDATE tracks SET duration_ms = ? WHERE file_path = ?");
    query.addBindValue(durationMs);
    query.addBindValue(QDir::toNativeSeparators(filePath));
    return query.exec();
}

bool MusicLibrary::setFavorite(const QString &filePath, bool favorite)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE tracks SET favorite = ? WHERE file_path = ?");
    query.addBindValue(favorite ? 1 : 0);
    query.addBindValue(QDir::toNativeSeparators(filePath));

    const bool ok = query.exec();
    if (ok) {
        emit libraryChanged();
    }
    return ok;
}

bool MusicLibrary::markPlayed(const QString &filePath)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE tracks SET last_played_at = ? WHERE file_path = ?");
    query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    query.addBindValue(QDir::toNativeSeparators(filePath));
    return query.exec();
}

QList<MusicTrack> MusicLibrary::loadQueue() const
{
    QList<MusicTrack> result;
    QSqlQuery query(m_db);
    query.prepare("SELECT t.id, t.provider_id, t.title, t.artist, t.album, t.file_path, t.duration_ms, t.favorite, t.added_at "
                  "FROM playback_queue q JOIN tracks t ON t.file_path = q.file_path "
                  "ORDER BY q.position ASC");

    if (!query.exec()) {
        return result;
    }

    while (query.next()) {
        result.append(trackFromQuery(query));
    }

    return result;
}

bool MusicLibrary::saveQueue(const QList<MusicTrack> &queue) const
{
    QSqlQuery clearQuery(m_db);
    if (!clearQuery.exec("DELETE FROM playback_queue")) {
        return false;
    }

    QSqlQuery insertQuery(m_db);
    insertQuery.prepare("INSERT INTO playback_queue(position, file_path) VALUES(?, ?)");

    int position = 0;
    for (const MusicTrack &track : queue) {
        insertQuery.bindValue(0, position++);
        insertQuery.bindValue(1, QDir::toNativeSeparators(track.filePath));
        if (!insertQuery.exec()) {
            return false;
        }
    }

    return true;
}

QString MusicLibrary::setting(const QString &key, const QString &fallback) const
{
    QSqlQuery query(m_db);
    query.prepare("SELECT value FROM settings WHERE key = ?");
    query.addBindValue(key);

    if (!query.exec() || !query.next()) {
        return fallback;
    }

    return query.value(0).toString();
}

bool MusicLibrary::setSetting(const QString &key, const QString &value) const
{
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO settings(key, value) VALUES(?, ?) "
                  "ON CONFLICT(key) DO UPDATE SET value = excluded.value");
    query.addBindValue(key);
    query.addBindValue(value);
    return query.exec();
}

bool MusicLibrary::ensureSchema()
{
    const QStringList statements = {
        "CREATE TABLE IF NOT EXISTS folders("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "path TEXT NOT NULL UNIQUE,"
        "added_at TEXT NOT NULL)",

        "CREATE TABLE IF NOT EXISTS tracks("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "provider_id TEXT NOT NULL DEFAULT 'local',"
        "title TEXT NOT NULL,"
        "artist TEXT,"
        "album TEXT,"
        "file_path TEXT NOT NULL UNIQUE,"
        "duration_ms INTEGER NOT NULL DEFAULT 0,"
        "favorite INTEGER NOT NULL DEFAULT 0,"
        "last_played_at TEXT,"
        "added_at TEXT NOT NULL)",

        "CREATE TABLE IF NOT EXISTS playback_queue("
        "position INTEGER NOT NULL,"
        "file_path TEXT NOT NULL UNIQUE)",

        "CREATE TABLE IF NOT EXISTS settings("
        "key TEXT PRIMARY KEY,"
        "value TEXT)"
    };

    for (const QString &statement : statements) {
        QSqlQuery query(m_db);
        if (!query.exec(statement)) {
            m_lastError = query.lastError().text();
            return false;
        }
    }

    return true;
}

bool MusicLibrary::upsertTrack(const MusicTrack &track) const
{
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO tracks(provider_id, title, artist, album, file_path, duration_ms, favorite, added_at) "
                  "VALUES(?, ?, ?, ?, ?, ?, ?, ?) "
                  "ON CONFLICT(file_path) DO UPDATE SET "
                  "title = excluded.title, "
                  "artist = CASE WHEN excluded.artist != '' THEN excluded.artist ELSE tracks.artist END, "
                  "album = CASE WHEN excluded.album != '' THEN excluded.album ELSE tracks.album END");
    query.addBindValue(track.providerId);
    query.addBindValue(track.title);
    query.addBindValue(track.artist);
    query.addBindValue(track.album);
    query.addBindValue(QDir::toNativeSeparators(track.filePath));
    query.addBindValue(track.durationMs);
    query.addBindValue(track.favorite ? 1 : 0);
    query.addBindValue(track.addedAt.isValid() ? track.addedAt.toString(Qt::ISODate)
                                               : QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    return query.exec();
}

MusicTrack MusicLibrary::trackFromQuery(const QSqlQuery &query) const
{
    MusicTrack track;
    track.id = query.value(0).toLongLong();
    track.providerId = query.value(1).toString();
    track.title = query.value(2).toString();
    track.artist = query.value(3).toString();
    track.album = query.value(4).toString();
    track.filePath = query.value(5).toString();
    track.durationMs = query.value(6).toLongLong();
    track.favorite = query.value(7).toInt() != 0;
    track.addedAt = QDateTime::fromString(query.value(8).toString(), Qt::ISODate);
    return track;
}

QList<MusicTrack> MusicLibrary::scanFolder(const QString &folderPath) const
{
    return m_localProvider.scanFolder(folderPath);
}
