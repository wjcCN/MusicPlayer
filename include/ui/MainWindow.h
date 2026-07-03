#pragma once

#include "core/PlaybackController.h"
#include "data/MusicLibrary.h"
#include "providers/ProviderRegistry.h"

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QTableWidget>
#include <QWidget>

class QCloseEvent;
class QGroupBox;

class MainWindow final : public QWidget
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    enum class Language
    {
        Chinese,
        English
    };

    enum class TextKey
    {
        WindowTitle,
        AddFolder,
        Rescan,
        SearchPlaceholder,
        LocalMusic,
        OfficialPlatformAccess,
        Settings,
        Language,
        Playback,
        NoTrackSelected,
        Ready,
        Previous,
        Play,
        Pause,
        Next,
        Mode,
        Volume,
        Sequential,
        LoopAll,
        LoopOne,
        Shuffle,
        Favorite,
        Title,
        Artist,
        Album,
        Length,
        Path,
        Platform,
        Status,
        Requirement,
        Unknown,
        Yes,
        AddMusicFolder,
        PlaybackProblem,
        LibraryError,
        LibraryOpenFailed,
        FolderScanned,
        RescanComplete,
        Playing,
        NowPlaying,
        LocalTracks,
        QqMusic,
        NeteaseMusic,
        KugouMusic,
        QqMusicRequirement,
        NeteaseRequirement,
        KugouRequirement,
        NotConfigured,
        Chinese,
        English
    };

    void buildUi();
    void connectSignals();
    void loadInitialState();
    void applyLanguage();
    void setLanguage(Language language);
    void refreshLibraryTable();
    void refreshPlatformTable();
    void setStatus(const QString &message);
    void applyPlaybackState(QMediaPlayer::PlaybackState state);
    void updatePositionLabel(qint64 positionMs, qint64 durationMs);
    void savePlaybackState();
    QString text(TextKey key) const;
    QString languageCode() const;
    Language languageFromCode(const QString &code) const;
    int rowToTrackIndex(int row) const;
    QString formatDuration(qint64 durationMs) const;

    MusicLibrary m_library;
    PlaybackController m_player;
    ProviderRegistry m_providerRegistry;

    QList<MusicTrack> m_allTracks;
    QList<MusicTrack> m_filteredTracks;
    qint64 m_currentDurationMs = 0;
    bool m_seeking = false;
    bool m_libraryReady = false;
    Language m_language = Language::Chinese;

    QLineEdit *m_searchEdit = nullptr;
    QLabel *m_libraryTitleLabel = nullptr;
    QLabel *m_platformTitleLabel = nullptr;
    QLabel *m_languageLabel = nullptr;
    QLabel *m_modeLabel = nullptr;
    QLabel *m_volumeLabel = nullptr;
    QTableWidget *m_libraryTable = nullptr;
    QTableWidget *m_platformTable = nullptr;
    QLabel *m_nowPlayingLabel = nullptr;
    QLabel *m_timeLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QSlider *m_positionSlider = nullptr;
    QSlider *m_volumeSlider = nullptr;
    QPushButton *m_addFolderButton = nullptr;
    QPushButton *m_rescanButton = nullptr;
    QPushButton *m_previousButton = nullptr;
    QPushButton *m_playPauseButton = nullptr;
    QPushButton *m_nextButton = nullptr;
    QComboBox *m_modeCombo = nullptr;
    QComboBox *m_languageCombo = nullptr;
    QGroupBox *m_playerBox = nullptr;
    QGroupBox *m_settingsBox = nullptr;
};
