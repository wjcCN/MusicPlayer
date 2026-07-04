#pragma once

#include "core/LyricsParser.h"
#include "core/PlaybackController.h"
#include "data/MusicLibrary.h"

#include <QComboBox>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QPixmap>
#include <QSlider>
#include <QTableWidget>
#include <QWidget>

class QCloseEvent;
class QEvent;
class FullscreenVideoWindow;
class QFrame;
class QGroupBox;
class QKeyEvent;
class QStackedWidget;
class QTimer;
class QVideoWidget;
class VideoStateOverlay;

class MainWindow final : public QWidget
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    enum class Language
    {
        Chinese,
        English
    };

    enum class Theme
    {
        Dark,
        Light
    };

    enum class TextKey
    {
        WindowTitle,
        AppSubtitle,
        AddFolder,
        ShowFolders,
        Rescan,
        SearchPlaceholder,
        LocalMusic,
        Settings,
        Language,
        Theme,
        SwitchToLight,
        SwitchToDark,
        Playback,
        Lyrics,
        NoTrackSelected,
        NoArtist,
        NoLyrics,
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
        Type,
        Title,
        Artist,
        Album,
        Length,
        Path,
        Audio,
        Video,
        Unknown,
        Yes,
        AddMusicFolder,
        AddedFolders,
        NoFolders,
        PlaybackProblem,
        LibraryError,
        LibraryOpenFailed,
        FolderScanned,
        RescanComplete,
        Playing,
        NowPlaying,
        LocalTracks,
        ShortcutHint,
        VideoControls,
        VideoScale,
        Fit,
        Fill,
        Stretch,
        Fullscreen,
        AudioOnly,
        ShowVideo,
        Chinese,
        English
    };

    void buildUi();
    void connectSignals();
    void installShortcuts();
    void loadInitialState();
    void applyLanguage();
    void setLanguage(Language language);
    void applyTheme();
    void setTheme(Theme theme);
    QString themeStyleSheet() const;
    void refreshLibraryTable();
    void setStatus(const QString &message);
    void applyPlaybackState(QMediaPlayer::PlaybackState state);
    void updatePositionLabel(qint64 positionMs, qint64 durationMs);
    void savePlaybackState();
    void showFoldersDialog();
    void loadTrackPresentation(const MusicTrack &track);
    void loadLyricsForTrack(const MusicTrack &track);
    void updateActiveLyric(qint64 positionMs);
    void showNoLyrics();
    void refreshMediaDisplay(const MusicTrack &track);
    void refreshVideoControls(const MusicTrack &track);
    void syncVideoOutput();
    void applyVideoScale();
    void toggleVideoFullscreen();
    void exitVideoFullscreen();
    bool handleVideoKeyEvent(QKeyEvent *event, bool pressed);
    void seekRelative(qint64 deltaMs);
    void setCoverFromImage(const QImage &image);
    void setCoverFromPixmap(const QPixmap &pixmap);
    QPixmap coverFromSidecarFile(const MusicTrack &track) const;
    QPixmap defaultCoverPixmap() const;
    QString text(TextKey key) const;
    QString languageCode() const;
    Language languageFromCode(const QString &code) const;
    QString themeCode() const;
    Theme themeFromCode(const QString &code) const;
    int rowToTrackIndex(int row) const;
    QString formatDuration(qint64 durationMs) const;

    MusicLibrary m_library;
    PlaybackController m_player;

    QList<MusicTrack> m_allTracks;
    QList<MusicTrack> m_filteredTracks;
    QList<LyricLine> m_lyrics;
    qint64 m_currentDurationMs = 0;
    int m_activeLyricIndex = -1;
    bool m_seeking = false;
    bool m_libraryReady = false;
    bool m_audioOnlyVideo = false;
    bool m_rightKeyPressed = false;
    bool m_rightLongPressActive = false;
    Language m_language = Language::Chinese;
    Theme m_theme = Theme::Dark;

    QLineEdit *m_searchEdit = nullptr;
    QLabel *m_appTitleLabel = nullptr;
    QLabel *m_appSubtitleLabel = nullptr;
    QLabel *m_libraryTitleLabel = nullptr;
    QLabel *m_languageLabel = nullptr;
    QLabel *m_themeLabel = nullptr;
    QLabel *m_modeLabel = nullptr;
    QLabel *m_volumeLabel = nullptr;
    QLabel *m_coverLabel = nullptr;
    QLabel *m_trackTitleLabel = nullptr;
    QLabel *m_trackArtistLabel = nullptr;
    QLabel *m_nowPlayingLabel = nullptr;
    QLabel *m_timeLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_lyricsTitleLabel = nullptr;
    QLabel *m_videoScaleLabel = nullptr;
    QListWidget *m_lyricsList = nullptr;
    QTableWidget *m_libraryTable = nullptr;
    QStackedWidget *m_mediaStack = nullptr;
    QVideoWidget *m_videoWidget = nullptr;
    VideoStateOverlay *m_videoStateOverlay = nullptr;
    FullscreenVideoWindow *m_fullscreenWindow = nullptr;
    QSlider *m_positionSlider = nullptr;
    QSlider *m_volumeSlider = nullptr;
    QPushButton *m_addFolderButton = nullptr;
    QPushButton *m_foldersButton = nullptr;
    QPushButton *m_rescanButton = nullptr;
    QPushButton *m_themeButton = nullptr;
    QPushButton *m_previousButton = nullptr;
    QPushButton *m_playPauseButton = nullptr;
    QPushButton *m_nextButton = nullptr;
    QPushButton *m_audioOnlyButton = nullptr;
    QPushButton *m_fullscreenButton = nullptr;
    QComboBox *m_modeCombo = nullptr;
    QComboBox *m_languageCombo = nullptr;
    QComboBox *m_videoScaleCombo = nullptr;
    QTimer *m_rightHoldTimer = nullptr;
    QGroupBox *m_playerBox = nullptr;
    QGroupBox *m_settingsBox = nullptr;
    QFrame *m_videoControlsFrame = nullptr;
};
