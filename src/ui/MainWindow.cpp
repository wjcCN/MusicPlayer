#include "ui/MainWindow.h"

#include "core/LyricsParser.h"
#include "ui/FolderManagerDialog.h"
#include "ui/FullscreenVideoWindow.h"
#include "ui/VideoStateOverlay.h"

#include <QAction>
#include <QAbstractItemView>
#include <QAudioOutput>
#include <QApplication>
#include <QBoxLayout>
#include <QCloseEvent>
#include <QDir>
#include <QDesktopServices>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QKeySequence>
#include <QListView>
#include <QMenu>
#include <QMediaMetaData>
#include <QMediaPlayer>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QProcess>
#include <QShortcut>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QStyle>
#include <QTableWidgetItem>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QVideoFrame>
#include <QVideoSink>
#include <QVideoWidget>

namespace
{
constexpr int FavoriteColumn = 0;
constexpr int TypeColumn = 1;
constexpr int TitleColumn = 2;
constexpr int ArtistColumn = 3;
constexpr int AlbumColumn = 4;
constexpr int DurationColumn = 5;
constexpr int PathColumn = 6;
constexpr int CoverSize = 300;
constexpr int TrackIndexRole = Qt::UserRole;
constexpr int TrackPathRole = Qt::UserRole + 1;
}

MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent)
{
    buildUi();
    connectSignals();
    installShortcuts();
    loadInitialState();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    savePlaybackState();
    QWidget::closeEvent(event);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateResponsiveLayout();
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    const bool isVideoEventSource = watched == m_videoWidget;
    if (!isVideoEventSource) {
        return QWidget::eventFilter(watched, event);
    }

    if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (qobject_cast<QLineEdit *>(focusWidget())) {
            return QWidget::eventFilter(watched, event);
        }
        if (handleVideoKeyEvent(keyEvent, event->type() == QEvent::KeyPress)) {
            return true;
        }
    }

    if (watched == m_videoWidget && event->type() == QEvent::MouseButtonDblClick) {
        toggleVideoFullscreen();
        return true;
    }

    if (watched == m_videoWidget && event->type() == QEvent::MouseButtonRelease) {
        m_player.togglePlayPause();
        return true;
    }

    return QWidget::eventFilter(watched, event);
}

void MainWindow::buildUi()
{
    setObjectName("appRoot");
    setWindowTitle(text(TextKey::WindowTitle));
    setMinimumSize(620, 360);
    resize(1360, 820);

    m_appTitleLabel = new QLabel(this);
    m_appTitleLabel->setObjectName("appTitle");
    m_appSubtitleLabel = new QLabel(this);
    m_appSubtitleLabel->setObjectName("appSubtitle");

    m_languageLabel = new QLabel(this);
    m_languageCombo = new QComboBox(this);
    m_languageCombo->addItem(QString(), QStringLiteral("zh"));
    m_languageCombo->addItem(QString(), QStringLiteral("en"));

    m_themeLabel = new QLabel(this);
    m_themeButton = new QPushButton(this);
    m_themeButton->setObjectName("ghostButton");

    auto *settingsLayout = new QHBoxLayout;
    settingsLayout->setContentsMargins(14, 10, 14, 10);
    settingsLayout->setSpacing(10);
    settingsLayout->addWidget(m_languageLabel);
    settingsLayout->addWidget(m_languageCombo);
    settingsLayout->addSpacing(8);
    settingsLayout->addWidget(m_themeLabel);
    settingsLayout->addWidget(m_themeButton);

    m_settingsBox = new QGroupBox(this);
    m_settingsBox->setObjectName("settingsBox");
    m_settingsBox->setLayout(settingsLayout);

    auto *headerTextLayout = new QVBoxLayout;
    headerTextLayout->setSpacing(3);
    headerTextLayout->addWidget(m_appTitleLabel);
    headerTextLayout->addWidget(m_appSubtitleLabel);

    m_headerLayout = new QBoxLayout(QBoxLayout::LeftToRight);
    m_headerLayout->setSpacing(16);
    m_headerLayout->addLayout(headerTextLayout, 1);
    m_headerLayout->addWidget(m_settingsBox);

    m_addFolderButton = new QPushButton(this);
    m_addFolderButton->setObjectName("primaryButton");
    m_foldersButton = new QPushButton(this);
    m_foldersButton->setObjectName("ghostButton");
    m_rescanButton = new QPushButton(this);
    m_rescanButton->setObjectName("ghostButton");
    m_folderFilterCombo = new QComboBox(this);
    m_folderFilterCombo->setObjectName("folderFilterCombo");
    m_folderFilterCombo->setMinimumWidth(190);
    m_libraryViewCombo = new QComboBox(this);
    m_libraryViewCombo->setObjectName("libraryViewCombo");
    m_libraryViewCombo->addItem(QString(), QStringLiteral("list"));
    m_libraryViewCombo->addItem(QString(), QStringLiteral("icons"));
    m_libraryViewCombo->setMinimumWidth(96);
    m_iconSizeCombo = new QComboBox(this);
    m_iconSizeCombo->setObjectName("iconSizeCombo");
    m_iconSizeCombo->addItem(QString(), QStringLiteral("medium"));
    m_iconSizeCombo->addItem(QString(), QStringLiteral("small"));
    m_iconSizeCombo->addItem(QString(), QStringLiteral("large"));
    m_iconSizeCombo->setMinimumWidth(96);
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setObjectName("searchEdit");
    m_searchEdit->setClearButtonEnabled(true);

    auto *toolbarLayout = new QHBoxLayout;
    toolbarLayout->setSpacing(10);
    toolbarLayout->addWidget(m_addFolderButton);
    toolbarLayout->addWidget(m_foldersButton);
    toolbarLayout->addWidget(m_rescanButton);
    toolbarLayout->addWidget(m_folderFilterCombo);
    toolbarLayout->addWidget(m_libraryViewCombo);
    toolbarLayout->addWidget(m_iconSizeCombo);
    toolbarLayout->addStretch(1);

    m_libraryTitleLabel = new QLabel(this);
    m_libraryTitleLabel->setObjectName("sectionTitle");

    m_libraryTable = new QTableWidget(this);
    m_libraryTable->setObjectName("libraryTable");
    m_libraryTable->setColumnCount(7);
    m_libraryTable->verticalHeader()->setVisible(false);
    m_libraryTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_libraryTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_libraryTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_libraryTable->setAlternatingRowColors(false);
    m_libraryTable->horizontalHeader()->setStretchLastSection(false);
    m_libraryTable->horizontalHeader()->setSectionResizeMode(TitleColumn, QHeaderView::Stretch);
    m_libraryTable->horizontalHeader()->setSectionResizeMode(PathColumn, QHeaderView::Interactive);
    m_libraryTable->setColumnWidth(FavoriteColumn, 58);
    m_libraryTable->setColumnWidth(TypeColumn, 72);
    m_libraryTable->setColumnWidth(DurationColumn, 90);
    m_libraryTable->setColumnWidth(PathColumn, 180);
    m_libraryTable->setShowGrid(false);
    m_libraryTable->verticalHeader()->setDefaultSectionSize(44);
    m_libraryTable->setContextMenuPolicy(Qt::CustomContextMenu);

    m_libraryIconList = new QListWidget(this);
    m_libraryIconList->setObjectName("libraryIconList");
    m_libraryIconList->setViewMode(QListView::IconMode);
    m_libraryIconList->setResizeMode(QListView::Adjust);
    m_libraryIconList->setMovement(QListView::Static);
    m_libraryIconList->setWrapping(true);
    m_libraryIconList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_libraryIconList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_libraryIconList->setContextMenuPolicy(Qt::CustomContextMenu);
    m_libraryIconList->setWordWrap(true);

    m_libraryViewStack = new QStackedWidget(this);
    m_libraryViewStack->setObjectName("libraryViewStack");
    m_libraryViewStack->addWidget(m_libraryTable);
    m_libraryViewStack->addWidget(m_libraryIconList);

    m_libraryPanel = new QFrame(this);
    m_libraryPanel->setObjectName("libraryPanel");
    m_libraryPanel->setMinimumWidth(0);
    auto *libraryLayout = new QVBoxLayout(m_libraryPanel);
    libraryLayout->setContentsMargins(18, 16, 18, 18);
    libraryLayout->setSpacing(12);
    libraryLayout->addWidget(m_libraryTitleLabel);
    libraryLayout->addLayout(toolbarLayout);
    libraryLayout->addWidget(m_searchEdit);
    libraryLayout->addWidget(m_libraryViewStack, 1);

    m_coverLabel = new QLabel(this);
    m_coverLabel->setObjectName("coverArt");
    m_coverLabel->setMinimumSize(180, 180);
    m_coverLabel->setAlignment(Qt::AlignCenter);
    m_coverLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_videoWidget = new QVideoWidget(this);
    m_videoWidget->setObjectName("videoSurface");
    m_videoWidget->setAspectRatioMode(Qt::KeepAspectRatio);
    m_videoWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_videoWidget->setFocusPolicy(Qt::StrongFocus);
    m_videoWidget->installEventFilter(this);
    qApp->installEventFilter(this);
    m_videoStateOverlay = new VideoStateOverlay(m_videoWidget);

    m_rightHoldTimer = new QTimer(this);
    m_rightHoldTimer->setSingleShot(true);
    connect(m_rightHoldTimer, &QTimer::timeout, this, [this] {
        if (!m_rightKeyPressed) {
            return;
        }
        m_rightLongPressActive = true;
        m_player.setPlaybackRate(2.0);
        setStatus(QStringLiteral("2x"));
    });

    m_mediaStack = new QStackedWidget(this);
    m_mediaStack->setObjectName("mediaStack");
    m_mediaStack->setMinimumSize(240, 180);
    m_mediaStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_mediaStack->addWidget(m_coverLabel);
    m_mediaStack->addWidget(m_videoWidget);

    m_trackTitleLabel = new QLabel(this);
    m_trackTitleLabel->setObjectName("trackTitle");
    m_trackTitleLabel->setAlignment(Qt::AlignCenter);
    m_trackTitleLabel->setWordWrap(true);
    m_trackArtistLabel = new QLabel(this);
    m_trackArtistLabel->setObjectName("trackArtist");
    m_trackArtistLabel->setAlignment(Qt::AlignCenter);
    m_trackArtistLabel->setWordWrap(true);

    m_videoScaleLabel = new QLabel(this);
    m_videoScaleCombo = new QComboBox(this);
    m_videoScaleCombo->addItem(QString(), 0);
    m_videoScaleCombo->addItem(QString(), 1);
    m_videoScaleCombo->addItem(QString(), 2);
    m_audioOnlyButton = new QPushButton(this);
    m_audioOnlyButton->setObjectName("ghostButton");
    m_audioOnlyButton->setCheckable(true);
    m_fullscreenButton = new QPushButton(this);
    m_fullscreenButton->setObjectName("ghostButton");

    auto *videoControlsLayout = new QHBoxLayout;
    videoControlsLayout->setContentsMargins(0, 0, 0, 0);
    videoControlsLayout->setSpacing(8);
    videoControlsLayout->addWidget(m_videoScaleLabel);
    videoControlsLayout->addWidget(m_videoScaleCombo, 1);
    videoControlsLayout->addWidget(m_audioOnlyButton);
    videoControlsLayout->addWidget(m_fullscreenButton);

    m_videoControlsFrame = new QFrame(this);
    m_videoControlsFrame->setObjectName("videoControls");
    m_videoControlsFrame->setLayout(videoControlsLayout);

    m_lyricsTitleLabel = new QLabel(this);
    m_lyricsTitleLabel->setObjectName("sectionTitle");

    m_lyricsList = new QListWidget(this);
    m_lyricsList->setObjectName("lyricsList");
    m_lyricsList->setSelectionMode(QAbstractItemView::NoSelection);
    m_lyricsList->setFocusPolicy(Qt::NoFocus);

    m_nowPlayingPanel = new QFrame(this);
    m_nowPlayingPanel->setObjectName("panel");
    m_nowPlayingPanel->setMinimumWidth(0);
    auto *nowPlayingLayout = new QVBoxLayout(m_nowPlayingPanel);
    nowPlayingLayout->setContentsMargins(22, 20, 22, 20);
    nowPlayingLayout->setSpacing(14);
    nowPlayingLayout->addWidget(m_mediaStack, 2);
    nowPlayingLayout->addWidget(m_videoControlsFrame);
    nowPlayingLayout->addWidget(m_trackTitleLabel);
    nowPlayingLayout->addWidget(m_trackArtistLabel);
    nowPlayingLayout->addSpacing(4);
    nowPlayingLayout->addWidget(m_lyricsTitleLabel);
    nowPlayingLayout->addWidget(m_lyricsList, 1);

    m_contentLayout = new QBoxLayout(QBoxLayout::LeftToRight);
    m_contentLayout->setSpacing(18);
    m_contentLayout->addWidget(m_libraryPanel, 3);
    m_contentLayout->addWidget(m_nowPlayingPanel, 2);

    m_nowPlayingLabel = new QLabel(this);
    m_nowPlayingLabel->setObjectName("nowPlaying");
    m_timeLabel = new QLabel(QStringLiteral("00:00 / 00:00"), this);
    m_timeLabel->setObjectName("timeLabel");
    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName("statusLabel");

    m_previousButton = new QPushButton(this);
    m_previousButton->setObjectName("transportButton");
    m_playPauseButton = new QPushButton(this);
    m_playPauseButton->setObjectName("playButton");
    m_nextButton = new QPushButton(this);
    m_nextButton->setObjectName("transportButton");

    m_positionSlider = new QSlider(Qt::Horizontal, this);
    m_positionSlider->setRange(0, 0);
    m_volumeSlider = new QSlider(Qt::Horizontal, this);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(70);
    m_volumeSlider->setMaximumWidth(170);

    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem(QString(), static_cast<int>(PlaybackMode::Sequential));
    m_modeCombo->addItem(QString(), static_cast<int>(PlaybackMode::LoopAll));
    m_modeCombo->addItem(QString(), static_cast<int>(PlaybackMode::LoopOne));
    m_modeCombo->addItem(QString(), static_cast<int>(PlaybackMode::Shuffle));

    m_modeLabel = new QLabel(this);
    m_volumeLabel = new QLabel(this);

    auto *transportLayout = new QHBoxLayout;
    transportLayout->setSpacing(10);
    transportLayout->addWidget(m_previousButton);
    transportLayout->addWidget(m_playPauseButton);
    transportLayout->addWidget(m_nextButton);
    transportLayout->addSpacing(14);
    transportLayout->addWidget(m_modeLabel);
    transportLayout->addWidget(m_modeCombo);
    transportLayout->addStretch();
    transportLayout->addWidget(m_volumeLabel);
    transportLayout->addWidget(m_volumeSlider);

    auto *playerLayout = new QVBoxLayout;
    playerLayout->setContentsMargins(18, 14, 18, 14);
    playerLayout->setSpacing(8);
    playerLayout->addWidget(m_nowPlayingLabel);
    playerLayout->addWidget(m_positionSlider);

    auto *statusLayout = new QHBoxLayout;
    statusLayout->addWidget(m_timeLabel);
    statusLayout->addStretch();
    statusLayout->addWidget(m_statusLabel);
    playerLayout->addLayout(statusLayout);
    playerLayout->addLayout(transportLayout);

    m_playerBox = new QGroupBox(this);
    m_playerBox->setObjectName("playerBox");
    m_playerBox->setLayout(playerLayout);

    auto *scrollContent = new QWidget(this);
    scrollContent->setObjectName("scrollContent");
    auto *scrollContentLayout = new QVBoxLayout(scrollContent);
    scrollContentLayout->setContentsMargins(0, 0, 0, 0);
    scrollContentLayout->setSpacing(18);
    scrollContentLayout->addLayout(m_headerLayout);
    scrollContentLayout->addLayout(m_contentLayout, 1);

    auto *mainScrollArea = new QScrollArea(this);
    mainScrollArea->setObjectName("mainScrollArea");
    mainScrollArea->setWidgetResizable(true);
    mainScrollArea->setFrameShape(QFrame::NoFrame);
    mainScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    mainScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    mainScrollArea->setWidget(scrollContent);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(22, 20, 22, 20);
    mainLayout->setSpacing(18);
    mainLayout->addWidget(mainScrollArea, 1);
    mainLayout->addWidget(m_playerBox);
    setLayout(mainLayout);

    setupArtworkExtractor();
    setCoverFromPixmap(defaultCoverPixmap());
    refreshVideoControls({});
    showNoLyrics();
    applyTheme();
    applyLanguage();
    updateResponsiveLayout();
}

void MainWindow::connectSignals()
{
    connect(m_addFolderButton, &QPushButton::clicked, this, [this] {
        const QString folder = QFileDialog::getExistingDirectory(this, text(TextKey::AddMusicFolder));
        if (folder.isEmpty()) {
            return;
        }

        const int changed = m_library.addFolder(folder);
        refreshFolderFilter(selectedFolderFilter());
        refreshLibraryTable();
        if (m_player.queue().isEmpty() && !m_allTracks.isEmpty()) {
            m_player.setQueue(m_allTracks);
        }
        setStatus(text(TextKey::FolderScanned).arg(changed));
    });

    connect(m_foldersButton, &QPushButton::clicked, this, &MainWindow::showFoldersDialog);

    connect(m_rescanButton, &QPushButton::clicked, this, [this] {
        const int changed = m_library.rescanFolders();
        refreshFolderFilter(selectedFolderFilter());
        refreshLibraryTable();
        if (m_player.queue().isEmpty() && !m_allTracks.isEmpty()) {
            m_player.setQueue(m_allTracks);
        }
        setStatus(text(TextKey::RescanComplete).arg(changed));
    });

    connect(m_searchEdit, &QLineEdit::textChanged, this, [this] {
        refreshLibraryTable();
    });

    connect(m_folderFilterCombo, &QComboBox::currentIndexChanged, this, [this] {
        if (m_libraryReady) {
            m_library.setSetting("folder_filter", selectedFolderFilter());
        }
        refreshLibraryTable();
    });

    connect(m_libraryViewCombo, &QComboBox::currentIndexChanged, this, [this] {
        applyLibraryView();
        if (m_libraryReady) {
            m_library.setSetting("library_view", m_libraryViewCombo->currentData().toString());
        }
    });

    connect(m_iconSizeCombo, &QComboBox::currentIndexChanged, this, [this] {
        applyIconSize();
        refreshLibraryIconView();
        if (m_libraryReady) {
            m_library.setSetting("icon_size", m_iconSizeCombo->currentData().toString());
        }
    });

    connect(m_libraryTable, &QTableWidget::cellDoubleClicked, this, [this](int row, int column) {
        const int index = rowToTrackIndex(row);
        if (index < 0) {
            return;
        }

        if (column == FavoriteColumn) {
            const MusicTrack track = m_filteredTracks.at(index);
            m_library.setFavorite(track.filePath, !track.favorite);
            refreshLibraryTable();
            return;
        }

        playTrackAt(index);
    });

    connect(m_libraryTable, &QTableWidget::customContextMenuRequested, this, [this](const QPoint &position) {
        showLibraryContextMenu(position, false);
    });

    connect(m_libraryIconList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        playTrackAt(iconItemToTrackIndex(item));
    });

    connect(m_libraryIconList, &QListWidget::customContextMenuRequested, this, [this](const QPoint &position) {
        showLibraryContextMenu(position, true);
    });

    connect(m_themeButton, &QPushButton::clicked, this, [this] {
        setTheme(m_theme == Theme::Dark ? Theme::Light : Theme::Dark);
    });

    connect(m_previousButton, &QPushButton::clicked, &m_player, &PlaybackController::previous);
    connect(m_playPauseButton, &QPushButton::clicked, &m_player, &PlaybackController::togglePlayPause);
    connect(m_nextButton, &QPushButton::clicked, &m_player, &PlaybackController::next);
    connect(m_volumeSlider, &QSlider::valueChanged, &m_player, &PlaybackController::setVolume);

    connect(m_modeCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        m_player.setPlaybackMode(static_cast<PlaybackMode>(m_modeCombo->itemData(index).toInt()));
    });
    connect(m_languageCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        setLanguage(languageFromCode(m_languageCombo->itemData(index).toString()));
    });
    connect(m_videoScaleCombo, &QComboBox::currentIndexChanged, this, [this] {
        applyVideoScale();
    });
    connect(m_audioOnlyButton, &QPushButton::toggled, this, [this](bool checked) {
        m_audioOnlyVideo = checked;
        syncVideoOutput();
        applyLanguage();
    });
    connect(m_fullscreenButton, &QPushButton::clicked, this, &MainWindow::toggleVideoFullscreen);

    connect(m_positionSlider, &QSlider::sliderPressed, this, [this] {
        m_seeking = true;
    });
    connect(m_positionSlider, &QSlider::sliderReleased, this, [this] {
        m_seeking = false;
        m_player.seek(m_positionSlider->value());
    });

    connect(&m_player, &PlaybackController::currentTrackChanged, this, [this](const MusicTrack &track) {
        loadTrackPresentation(track);
        m_library.markPlayed(track.filePath);
        setStatus(text(TextKey::Playing).arg(track.displayTitle()));
    });
    connect(&m_player, &PlaybackController::coverArtChanged, this, [this](const QImage &image) {
        setCoverFromImage(image);
    });
    connect(&m_player, &PlaybackController::playbackStateChanged, this, &MainWindow::applyPlaybackState);
    connect(&m_player, &PlaybackController::positionChanged, this, [this](qint64 position) {
        if (!m_seeking) {
            m_positionSlider->setValue(static_cast<int>(position));
        }
        updatePositionLabel(position, m_currentDurationMs);
        updateActiveLyric(position);
    });
    connect(&m_player, &PlaybackController::durationChanged, this, [this](qint64 duration) {
        m_currentDurationMs = duration;
        m_positionSlider->setRange(0, static_cast<int>(duration));
        updatePositionLabel(m_positionSlider->value(), duration);
    });
    connect(&m_player, &PlaybackController::durationLearned, this, [this](const MusicTrack &track, qint64 duration) {
        if (m_library.setTrackDuration(track.filePath, duration)) {
            refreshLibraryTable();
        }
    });
    connect(&m_player, &PlaybackController::errorOccurred, this, [this](const QString &message) {
        setStatus(message);
        QMessageBox::warning(this, text(TextKey::PlaybackProblem), message);
    });
    connect(&m_player, &PlaybackController::queueChanged, this, [this](const QList<MusicTrack> &queue) {
        m_library.saveQueue(queue);
    });
    connect(&m_library, &MusicLibrary::statusMessage, this, &MainWindow::setStatus);
}

void MainWindow::installShortcuts()
{
    auto *spaceShortcut = new QShortcut(QKeySequence(Qt::Key_Space), this);
    spaceShortcut->setContext(Qt::WindowShortcut);
    connect(spaceShortcut, &QShortcut::activated, this, [this] {
        if (qobject_cast<QLineEdit *>(focusWidget())) {
            return;
        }
        m_player.togglePlayPause();
    });

    auto *mediaToggleShortcut = new QShortcut(QKeySequence(Qt::Key_MediaTogglePlayPause), this);
    connect(mediaToggleShortcut, &QShortcut::activated, &m_player, &PlaybackController::togglePlayPause);

    auto *previousShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Left), this);
    connect(previousShortcut, &QShortcut::activated, &m_player, &PlaybackController::previous);

    auto *nextShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Right), this);
    connect(nextShortcut, &QShortcut::activated, &m_player, &PlaybackController::next);
}

void MainWindow::loadInitialState()
{
    if (!m_library.open()) {
        setStatus(text(TextKey::LibraryOpenFailed).arg(m_library.lastError()));
        QMessageBox::critical(this, text(TextKey::LibraryError), m_library.lastError());
        return;
    }

    m_libraryReady = true;
    setLanguage(languageFromCode(m_library.setting("language", "zh")));
    setTheme(themeFromCode(m_library.setting("theme", "dark")));

    const int volume = m_library.setting("volume", "70").toInt();
    m_volumeSlider->setValue(qBound(0, volume, 100));
    m_player.setVolume(m_volumeSlider->value());

    const int modeValue = m_library.setting("playback_mode", QString::number(static_cast<int>(PlaybackMode::Sequential))).toInt();
    const int modeIndex = m_modeCombo->findData(modeValue);
    if (modeIndex >= 0) {
        m_modeCombo->setCurrentIndex(modeIndex);
    }

    const int videoScale = m_library.setting("video_scale", "0").toInt();
    const int videoScaleIndex = m_videoScaleCombo->findData(videoScale);
    if (videoScaleIndex >= 0) {
        m_videoScaleCombo->setCurrentIndex(videoScaleIndex);
    }
    m_audioOnlyVideo = m_library.setting("video_audio_only", "0") == "1";
    m_audioOnlyButton->setChecked(m_audioOnlyVideo);
    applyVideoScale();

    const int libraryViewIndex = m_libraryViewCombo->findData(m_library.setting("library_view", "list"));
    if (libraryViewIndex >= 0) {
        m_libraryViewCombo->setCurrentIndex(libraryViewIndex);
    }
    const int iconSizeIndex = m_iconSizeCombo->findData(m_library.setting("icon_size", "medium"));
    if (iconSizeIndex >= 0) {
        m_iconSizeCombo->setCurrentIndex(iconSizeIndex);
    }
    applyIconSize();
    applyLibraryView();

    refreshFolderFilter(m_library.setting("folder_filter"));
    refreshLibraryTable();

    const QList<MusicTrack> savedQueue = m_library.loadQueue();
    if (!savedQueue.isEmpty()) {
        m_player.setQueue(savedQueue);
    } else {
        m_player.setQueue(m_allTracks);
    }
}

void MainWindow::applyLanguage()
{
    setWindowTitle(text(TextKey::WindowTitle));
    m_appTitleLabel->setText(text(TextKey::WindowTitle));
    m_appSubtitleLabel->setText(text(TextKey::AppSubtitle));
    m_addFolderButton->setText(text(TextKey::AddFolder));
    m_foldersButton->setText(text(TextKey::ShowFolders));
    m_rescanButton->setText(text(TextKey::Rescan));
    refreshFolderFilter(selectedFolderFilter());
    m_searchEdit->setPlaceholderText(text(TextKey::SearchPlaceholder));
    m_libraryTitleLabel->setText(text(TextKey::LocalMusic));
    m_settingsBox->setTitle(text(TextKey::Settings));
    m_languageLabel->setText(text(TextKey::Language));
    m_themeLabel->setText(text(TextKey::Theme));
    m_themeButton->setText(text(m_theme == Theme::Dark ? TextKey::SwitchToDark : TextKey::SwitchToLight));
    m_playerBox->setTitle(text(TextKey::Playback));
    m_modeLabel->setText(text(TextKey::Mode));
    m_volumeLabel->setText(text(TextKey::Volume));
    m_previousButton->setText(text(TextKey::Previous));
    m_nextButton->setText(text(TextKey::Next));
    m_lyricsTitleLabel->setText(text(TextKey::Lyrics));
    m_videoScaleLabel->setText(text(TextKey::VideoScale));
    m_audioOnlyButton->setText(text(m_audioOnlyVideo ? TextKey::ShowVideo : TextKey::AudioOnly));
    m_fullscreenButton->setText(text(TextKey::Fullscreen));
    if (m_fullscreenWindow) {
        m_fullscreenWindow->setEnglish(m_language == Language::English);
    }

    m_libraryTable->setHorizontalHeaderLabels({
        text(TextKey::Favorite),
        text(TextKey::Type),
        text(TextKey::Title),
        text(TextKey::Artist),
        text(TextKey::Album),
        text(TextKey::Length),
        text(TextKey::Path)
    });

    {
        const QSignalBlocker blocker(m_languageCombo);
        m_languageCombo->setItemText(0, text(TextKey::Chinese));
        m_languageCombo->setItemText(1, text(TextKey::English));
        const int languageIndex = m_languageCombo->findData(languageCode());
        if (languageIndex >= 0) {
            m_languageCombo->setCurrentIndex(languageIndex);
        }
    }

    {
        const QSignalBlocker blocker(m_modeCombo);
        const int currentData = m_modeCombo->currentData().toInt();
        m_modeCombo->setItemText(0, text(TextKey::Sequential));
        m_modeCombo->setItemText(1, text(TextKey::LoopAll));
        m_modeCombo->setItemText(2, text(TextKey::LoopOne));
        m_modeCombo->setItemText(3, text(TextKey::Shuffle));
        const int modeIndex = m_modeCombo->findData(currentData);
        if (modeIndex >= 0) {
            m_modeCombo->setCurrentIndex(modeIndex);
        }
    }

    {
        const QSignalBlocker blocker(m_videoScaleCombo);
        const int currentData = m_videoScaleCombo->currentData().toInt();
        m_videoScaleCombo->setItemText(0, text(TextKey::Fit));
        m_videoScaleCombo->setItemText(1, text(TextKey::Fill));
        m_videoScaleCombo->setItemText(2, text(TextKey::Stretch));
        const int scaleIndex = m_videoScaleCombo->findData(currentData);
        if (scaleIndex >= 0) {
            m_videoScaleCombo->setCurrentIndex(scaleIndex);
        }
    }

    {
        const QSignalBlocker blocker(m_libraryViewCombo);
        const QString currentData = m_libraryViewCombo->currentData().toString();
        m_libraryViewCombo->setItemText(0, text(TextKey::ListView));
        m_libraryViewCombo->setItemText(1, text(TextKey::IconView));
        const int viewIndex = m_libraryViewCombo->findData(currentData);
        if (viewIndex >= 0) {
            m_libraryViewCombo->setCurrentIndex(viewIndex);
        }
    }

    {
        const QSignalBlocker blocker(m_iconSizeCombo);
        const QString currentData = m_iconSizeCombo->currentData().toString();
        m_iconSizeCombo->setItemText(0, text(TextKey::MediumIcons));
        m_iconSizeCombo->setItemText(1, text(TextKey::SmallIcons));
        m_iconSizeCombo->setItemText(2, text(TextKey::LargeIcons));
        const int sizeIndex = m_iconSizeCombo->findData(currentData);
        if (sizeIndex >= 0) {
            m_iconSizeCombo->setCurrentIndex(sizeIndex);
        }
    }

    const MusicTrack currentTrack = m_player.currentTrack();
    if (currentTrack.filePath.isEmpty()) {
        m_nowPlayingLabel->setText(text(TextKey::NoTrackSelected));
        m_trackTitleLabel->setText(text(TextKey::NoTrackSelected));
        m_trackArtistLabel->setText(text(TextKey::ShortcutHint));
    } else {
        m_nowPlayingLabel->setText(text(TextKey::NowPlaying).arg(currentTrack.displayTitle()));
        m_trackTitleLabel->setText(currentTrack.displayTitle());
        m_trackArtistLabel->setText(currentTrack.artist.isEmpty() ? text(TextKey::NoArtist) : currentTrack.artist);
    }

    applyPlaybackState(m_player.playbackState());
    if (m_libraryReady) {
        refreshLibraryTable();
    }
    if (m_lyrics.isEmpty()) {
        showNoLyrics();
    }
    if (m_statusLabel->text().isEmpty()) {
        setStatus(text(TextKey::Ready));
    }
}

void MainWindow::setLanguage(Language language)
{
    if (m_language == language) {
        applyLanguage();
        return;
    }

    m_language = language;
    if (m_libraryReady) {
        m_library.setSetting("language", languageCode());
    }
    applyLanguage();
}

void MainWindow::applyTheme()
{
    qApp->setStyleSheet({});
    qApp->setStyleSheet(themeStyleSheet());

    const QWidgetList widgets = qApp->allWidgets();
    for (QWidget *widget : widgets) {
        if (!widget) {
            continue;
        }
        widget->style()->unpolish(widget);
        widget->style()->polish(widget);
        widget->update();
    }

    if (m_themeButton) {
        m_themeButton->setText(text(m_theme == Theme::Dark ? TextKey::SwitchToDark : TextKey::SwitchToLight));
    }
    m_activeLyricIndex = -1;
    updateActiveLyric(m_positionSlider ? m_positionSlider->value() : 0);
}

QString MainWindow::themeStyleSheet() const
{
    QFile styleFile(":/styles/app.qss");
    if (!styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    QString style = QString::fromUtf8(styleFile.readAll());
    const bool light = m_theme == Theme::Light;

    style.replace("@buttonSurface", ":/images/button-surface.png");
    style.replace("@buttonReplacement", light ? ":/images/light-button-round.png" : ":/images/button-replacement.png");
    style.replace("@mainBackground", light ? ":/images/light-main-background.png" : ":/images/main-background.png");
    style.replace("@dialogBackground", light ? ":/images/light-dialog-background.png" : ":/images/dialog-background.png");
    style.replace("@sidebarPanel", light ? ":/images/light-sidebar-panel.png" : ":/images/sidebar-panel.png");
    style.replace("@borderStrong", light ? "#b9c8d9" : "#31506a");
    style.replace("@buttonHover", light ? "#eef4fa" : "#29384f");
    style.replace("@buttonText", light ? "#1f2937" : "#eef4fb");
    style.replace("@accentText", light ? "#0f766e" : "#f8ffff");
    style.replace("@accentSoft", light ? "#ccfbf1" : "#1d5b62");
    style.replace("@selectionBg", light ? "rgba(204, 251, 241, 150)" : "rgba(45, 212, 191, 92)");
    style.replace("@menuBg", light ? "#ffffff" : "#101826");
    style.replace("@menuItemHover", light ? "#e6f7f4" : "#1e3144");
    style.replace("@fullscreenWindowBg", light ? "#10141d" : "#03060b");
    style.replace("@fullscreenFrameBg", light ? "#111827" : "#050914");
    style.replace("@fullscreenControlBg", light ? "rgba(17, 24, 39, 232)" : "rgba(8, 13, 20, 232)");
    style.replace("@fullscreenBorder", light ? "rgba(148, 163, 184, 140)" : "rgba(45, 212, 191, 95)");
    style.replace("@lyricActive", light ? "#0f766e" : "#f7d88f");
    style.replace("@windowBg", light ? "#f4f7fb" : "#0b1018");
    style.replace("@panelDeep", light ? "#eaf0f7" : "#080d14");
    style.replace("@panelAlt", light ? "#eef4fa" : "#0f1724");
    style.replace("@panelBg", light ? "#ffffff" : "#131b27");
    style.replace("@ghostBg", light ? "#ffffff" : "#111827");
    style.replace("@heading", light ? "#111827" : "#f8fafc");
    style.replace("@buttonBg", light ? "#f8fafc" : "#1e293b");
    style.replace("@border", light ? "#dbe4f0" : "#253246");
    style.replace("@accent", light ? "#0f766e" : "#2dd4bf");
    style.replace("@muted", light ? "#64748b" : "#90a0b7");
    style.replace("@error", light ? "#b91c1c" : "#fca5a5");
    style.replace("@text", light ? "#16202f" : "#eef4fb");
    style.replace("@ok", light ? "#0f766e" : "#5eead4");
    return style;
}

void MainWindow::refreshFolderFilter(const QString &preferredPath)
{
    if (!m_folderFilterCombo) {
        return;
    }

    const QString currentPath = preferredPath.isNull() ? selectedFolderFilter() : preferredPath;
    const QStringList folders = m_libraryReady ? m_library.folders() : QStringList();

    QSignalBlocker blocker(m_folderFilterCombo);
    m_folderFilterCombo->clear();
    m_folderFilterCombo->addItem(text(TextKey::AllFolders), QString());

    int selectedIndex = 0;
    for (const QString &folder : folders) {
        const int row = m_folderFilterCombo->count();
        m_folderFilterCombo->addItem(QDir::toNativeSeparators(folder), folder);
        if (!currentPath.isEmpty() && folder.compare(currentPath, Qt::CaseInsensitive) == 0) {
            selectedIndex = row;
        }
    }

    m_folderFilterCombo->setCurrentIndex(selectedIndex);
}

void MainWindow::setTheme(Theme theme)
{
    if (m_theme == theme) {
        applyTheme();
        return;
    }

    m_theme = theme;
    if (m_libraryReady) {
        m_library.setSetting("theme", themeCode());
    }
    applyTheme();
    applyLanguage();
}

void MainWindow::refreshLibraryTable()
{
    m_allTracks = m_library.tracks();
    const QString filter = m_searchEdit->text().trimmed();
    const QString folderFilter = selectedFolderFilter();
    m_filteredTracks.clear();

    for (const MusicTrack &track : m_allTracks) {
        if (!trackMatchesFolderFilter(track, folderFilter)) {
            continue;
        }

        const QString mediaType = track.isVideo ? text(TextKey::Video) : text(TextKey::Audio);
        const QString haystack = QString("%1 %2 %3 %4 %5").arg(track.title, track.artist, track.album, track.filePath, mediaType);
        if (filter.isEmpty() || haystack.contains(filter, Qt::CaseInsensitive)) {
            m_filteredTracks.append(track);
        }
    }

    m_libraryTable->setRowCount(m_filteredTracks.size());
    for (int row = 0; row < m_filteredTracks.size(); ++row) {
        const MusicTrack &track = m_filteredTracks.at(row);
        const QFileInfo fileInfo(track.filePath);

        auto *favoriteItem = new QTableWidgetItem(track.favorite ? text(TextKey::Yes) : QString());
        favoriteItem->setTextAlignment(Qt::AlignCenter);
        m_libraryTable->setItem(row, FavoriteColumn, favoriteItem);

        auto *typeItem = new QTableWidgetItem(track.isVideo ? text(TextKey::Video) : text(TextKey::Audio));
        typeItem->setTextAlignment(Qt::AlignCenter);
        m_libraryTable->setItem(row, TypeColumn, typeItem);

        m_libraryTable->setItem(row, TitleColumn, new QTableWidgetItem(track.displayTitle()));
        m_libraryTable->setItem(row, ArtistColumn, new QTableWidgetItem(track.artist.isEmpty() ? text(TextKey::Unknown) : track.artist));
        m_libraryTable->setItem(row, AlbumColumn, new QTableWidgetItem(track.album.isEmpty() ? text(TextKey::Unknown) : track.album));
        m_libraryTable->setItem(row, DurationColumn, new QTableWidgetItem(formatDuration(track.durationMs)));
        m_libraryTable->setItem(row, PathColumn, new QTableWidgetItem(QDir::toNativeSeparators(fileInfo.absoluteFilePath())));

        if (!fileInfo.exists()) {
            for (int column = 0; column < m_libraryTable->columnCount(); ++column) {
                if (auto *item = m_libraryTable->item(row, column)) {
                    item->setForeground(QColor(m_theme == Theme::Dark ? "#7f8da3" : "#9aa4b2"));
                }
            }
        }
    }

    setStatus(text(TextKey::LocalTracks).arg(m_filteredTracks.size()));
    refreshLibraryIconView();
}

void MainWindow::refreshLibraryIconView()
{
    if (!m_libraryIconList) {
        return;
    }

    m_libraryIconList->clear();
    const QSize iconSize = selectedIconExtent();
    m_libraryIconList->setIconSize(iconSize);
    m_libraryIconList->setGridSize(QSize(iconSize.width() + 58, iconSize.height() + 88));

    for (int index = 0; index < m_filteredTracks.size(); ++index) {
        const MusicTrack &track = m_filteredTracks.at(index);
        const QString artist = track.artist.isEmpty() ? text(TextKey::Unknown) : track.artist;
        const QString label = QStringLiteral("%1\n%2\n%3")
                                  .arg(track.displayTitle(), artist, formatDuration(track.durationMs));

        auto *item = new QListWidgetItem(iconForTrack(track), label);
        item->setTextAlignment(Qt::AlignHCenter);
        item->setData(TrackIndexRole, index);
        item->setData(TrackPathRole, artworkKey(track));
        item->setToolTip(QDir::toNativeSeparators(track.filePath));
        item->setSizeHint(QSize(iconSize.width() + 50, iconSize.height() + 78));
        if (!QFileInfo::exists(track.filePath)) {
            item->setForeground(QColor(m_theme == Theme::Dark ? "#7f8da3" : "#9aa4b2"));
        }
        m_libraryIconList->addItem(item);
    }
}

void MainWindow::applyLibraryView()
{
    if (!m_libraryViewStack) {
        return;
    }

    const bool iconView = selectedLibraryView() == LibraryView::Icons;
    m_libraryViewStack->setCurrentWidget(iconView ? static_cast<QWidget *>(m_libraryIconList)
                                                  : static_cast<QWidget *>(m_libraryTable));
    if (m_iconSizeCombo) {
        m_iconSizeCombo->setEnabled(iconView);
    }
}

void MainWindow::applyIconSize()
{
    if (!m_libraryIconList) {
        return;
    }

    const QSize iconSize = selectedIconExtent();
    m_libraryIconList->setIconSize(iconSize);
    m_libraryIconList->setGridSize(QSize(iconSize.width() + 58, iconSize.height() + 88));
}

void MainWindow::setupArtworkExtractor()
{
    m_artworkPlayer = new QMediaPlayer(this);
    m_artworkAudioOutput = new QAudioOutput(this);
    m_artworkVideoSink = new QVideoSink(this);
    m_artworkTimeoutTimer = new QTimer(this);
    m_artworkTimeoutTimer->setSingleShot(true);

    m_artworkAudioOutput->setMuted(true);
    m_artworkAudioOutput->setVolume(0.0f);
    m_artworkPlayer->setAudioOutput(m_artworkAudioOutput);
    m_artworkPlayer->setVideoOutput(m_artworkVideoSink);

    connect(m_artworkPlayer, &QMediaPlayer::metaDataChanged, this, [this] {
        if (m_artworkCurrentKey.isEmpty() || m_artworkCurrentTrack.isVideo) {
            return;
        }

        const QMediaMetaData metaData = m_artworkPlayer->metaData();
        QImage cover = metaData.value(QMediaMetaData::CoverArtImage).value<QImage>();
        if (cover.isNull()) {
            cover = metaData.value(QMediaMetaData::ThumbnailImage).value<QImage>();
        }
        if (!cover.isNull()) {
            finishArtworkRequest(QPixmap::fromImage(cover));
        }
    });

    connect(m_artworkVideoSink, &QVideoSink::videoFrameChanged, this, [this](const QVideoFrame &frame) {
        if (m_artworkCurrentKey.isEmpty() || !m_artworkCurrentTrack.isVideo || !m_artworkAcceptFrames || !frame.isValid()) {
            return;
        }

        const QImage frameImage = frame.toImage();
        if (!frameImage.isNull()) {
            finishArtworkRequest(QPixmap::fromImage(frameImage));
        }
    });

    connect(m_artworkPlayer, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
        if (m_artworkCurrentKey.isEmpty()) {
            return;
        }
        if ((status == QMediaPlayer::LoadedMedia || status == QMediaPlayer::BufferedMedia) && m_artworkCurrentTrack.isVideo) {
            m_artworkAcceptFrames = true;
            m_artworkPlayer->setPosition(0);
            m_artworkPlayer->play();
        } else if (status == QMediaPlayer::InvalidMedia || status == QMediaPlayer::EndOfMedia) {
            finishArtworkRequest();
        }
    });

    connect(m_artworkPlayer, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error, const QString &) {
        finishArtworkRequest();
    });

    connect(m_artworkTimeoutTimer, &QTimer::timeout, this, [this] {
        finishArtworkRequest();
    });
}

void MainWindow::requestArtwork(const MusicTrack &track)
{
    const QString key = artworkKey(track);
    if (key.isEmpty() || m_artworkCache.contains(key) || m_artworkRequested.contains(key)) {
        return;
    }

    if (!QFileInfo::exists(track.filePath)) {
        m_artworkRequested.insert(key);
        return;
    }

    if (!track.isVideo) {
        const QPixmap sidecarCover = coverFromSidecarFile(track);
        if (!sidecarCover.isNull()) {
            m_artworkCache.insert(key, sidecarCover);
            m_artworkRequested.insert(key);
            updateIconForPath(key);
            return;
        }
    }

    m_artworkRequested.insert(key);
    m_artworkQueue.append(track);
    startNextArtworkRequest();
}

void MainWindow::startNextArtworkRequest()
{
    if (!m_artworkCurrentKey.isEmpty() || !m_artworkPlayer || m_artworkQueue.isEmpty()) {
        return;
    }

    m_artworkCurrentTrack = m_artworkQueue.takeFirst();
    m_artworkCurrentKey = artworkKey(m_artworkCurrentTrack);
    m_artworkAcceptFrames = false;
    const QFileInfo fileInfo(m_artworkCurrentTrack.filePath);
    if (m_artworkCurrentKey.isEmpty() || !fileInfo.exists()) {
        m_artworkCurrentTrack = {};
        m_artworkCurrentKey.clear();
        startNextArtworkRequest();
        return;
    }

    m_artworkPlayer->stop();
    if (m_artworkVideoSink) {
        m_artworkVideoSink->setVideoFrame({});
    }
    m_artworkPlayer->setSource(QUrl::fromLocalFile(fileInfo.absoluteFilePath()));
    if (!m_artworkCurrentTrack.isVideo) {
        m_artworkPlayer->play();
    }
    m_artworkTimeoutTimer->start(m_artworkCurrentTrack.isVideo ? 4000 : 2500);
}

void MainWindow::finishArtworkRequest(const QPixmap &pixmap)
{
    if (m_artworkCurrentKey.isEmpty()) {
        return;
    }

    const QString completedKey = m_artworkCurrentKey;
    if (!pixmap.isNull()) {
        m_artworkCache.insert(completedKey, pixmap);
    }

    m_artworkCurrentTrack = {};
    m_artworkCurrentKey.clear();
    m_artworkAcceptFrames = false;

    if (m_artworkTimeoutTimer) {
        m_artworkTimeoutTimer->stop();
    }
    if (m_artworkPlayer) {
        m_artworkPlayer->stop();
        m_artworkPlayer->setSource({});
    }

    updateIconForPath(completedKey);
    startNextArtworkRequest();
}

void MainWindow::updateIconForPath(const QString &filePath)
{
    if (!m_libraryIconList || filePath.isEmpty()) {
        return;
    }

    for (int row = 0; row < m_libraryIconList->count(); ++row) {
        QListWidgetItem *item = m_libraryIconList->item(row);
        if (!item || item->data(TrackPathRole).toString().compare(filePath, Qt::CaseInsensitive) != 0) {
            continue;
        }

        const int index = iconItemToTrackIndex(item);
        if (index >= 0) {
            item->setIcon(iconForTrack(m_filteredTracks.at(index)));
        }
    }
}

void MainWindow::setStatus(const QString &message)
{
    m_statusLabel->setText(message);
}

void MainWindow::applyPlaybackState(QMediaPlayer::PlaybackState state)
{
    m_playPauseButton->setText(state == QMediaPlayer::PlayingState ? text(TextKey::Pause) : text(TextKey::Play));

    const MusicTrack track = m_player.currentTrack();
    const bool showVideoFeedback = track.isVideo && !m_audioOnlyVideo && m_videoStateOverlay;
    if (!showVideoFeedback) {
        if (m_videoStateOverlay) {
            m_videoStateOverlay->hideIndicator();
        }
        return;
    }

    if (state == QMediaPlayer::PlayingState) {
        m_videoStateOverlay->showPlayingIndicator();
    } else if (state == QMediaPlayer::PausedState) {
        m_videoStateOverlay->showPausedIndicator();
    } else {
        m_videoStateOverlay->hideIndicator();
    }
}

void MainWindow::updatePositionLabel(qint64 positionMs, qint64 durationMs)
{
    m_timeLabel->setText(QString("%1 / %2").arg(formatDuration(positionMs), formatDuration(durationMs)));
}

void MainWindow::savePlaybackState()
{
    if (!m_libraryReady) {
        return;
    }

    m_library.saveQueue(m_player.queue());
    m_library.setSetting("volume", QString::number(m_volumeSlider->value()));
    m_library.setSetting("playback_mode", QString::number(static_cast<int>(m_player.playbackMode())));
    m_library.setSetting("language", languageCode());
    m_library.setSetting("theme", themeCode());
    m_library.setSetting("folder_filter", selectedFolderFilter());
    m_library.setSetting("library_view", m_libraryViewCombo->currentData().toString());
    m_library.setSetting("icon_size", m_iconSizeCombo->currentData().toString());
    m_library.setSetting("video_scale", QString::number(m_videoScaleCombo->currentData().toInt()));
    m_library.setSetting("video_audio_only", m_audioOnlyVideo ? "1" : "0");
}

void MainWindow::showFoldersDialog()
{
    if (!m_libraryReady) {
        setStatus(text(TextKey::LibraryOpenFailed).arg(m_library.lastError()));
        return;
    }

    FolderManagerDialog dialog(&m_library, m_language == Language::English, this);
    connect(&dialog, &FolderManagerDialog::foldersChanged, this, [this] {
        refreshFolderFilter(selectedFolderFilter());
        refreshLibraryTable();
        if (m_player.queue().isEmpty() && !m_allTracks.isEmpty()) {
            m_player.setQueue(m_allTracks);
        }
    });
    dialog.exec();
}

void MainWindow::loadTrackPresentation(const MusicTrack &track)
{
    m_trackTitleLabel->setText(track.displayTitle());
    m_trackArtistLabel->setText(track.artist.isEmpty() ? text(TextKey::NoArtist) : track.artist);
    m_nowPlayingLabel->setText(text(TextKey::NowPlaying).arg(track.displayTitle()));

    const QPixmap sidecarCover = coverFromSidecarFile(track);
    setCoverFromPixmap(sidecarCover.isNull() ? defaultCoverPixmap() : sidecarCover);
    refreshMediaDisplay(track);
    loadLyricsForTrack(track);
}

void MainWindow::loadLyricsForTrack(const MusicTrack &track)
{
    m_lyrics = LyricsParser::parseLrcFile(LyricsParser::lrcPathForAudioFile(track.filePath));
    m_activeLyricIndex = -1;
    m_lyricsList->clear();

    if (m_lyrics.isEmpty()) {
        showNoLyrics();
        return;
    }

    for (const LyricLine &line : m_lyrics) {
        auto *item = new QListWidgetItem(line.text);
        item->setTextAlignment(Qt::AlignCenter);
        item->setForeground(QColor(m_theme == Theme::Dark ? "#9ba8be" : "#68768a"));
        m_lyricsList->addItem(item);
    }

    updateActiveLyric(0);
}

void MainWindow::updateActiveLyric(qint64 positionMs)
{
    if (m_lyrics.isEmpty()) {
        return;
    }

    int activeIndex = -1;
    for (int i = 0; i < m_lyrics.size(); ++i) {
        if (m_lyrics.at(i).timeMs <= positionMs + 120) {
            activeIndex = i;
        } else {
            break;
        }
    }

    if (activeIndex < 0 || activeIndex == m_activeLyricIndex) {
        return;
    }

    for (int i = 0; i < m_lyricsList->count(); ++i) {
        QListWidgetItem *item = m_lyricsList->item(i);
        QFont font = item->font();
        font.setBold(i == activeIndex);
        font.setPointSize(i == activeIndex ? 14 : 11);
        item->setFont(font);
        item->setForeground(i == activeIndex
                                ? QColor(m_theme == Theme::Dark ? "#f7d88f" : "#0f766e")
                                : QColor(m_theme == Theme::Dark ? "#9ba8be" : "#68768a"));
    }

    m_activeLyricIndex = activeIndex;
    m_lyricsList->setCurrentRow(activeIndex);
    m_lyricsList->scrollToItem(m_lyricsList->item(activeIndex), QAbstractItemView::PositionAtCenter);
}

void MainWindow::showNoLyrics()
{
    m_activeLyricIndex = -1;
    m_lyricsList->clear();
    auto *item = new QListWidgetItem(text(TextKey::NoLyrics));
    item->setTextAlignment(Qt::AlignCenter);
    item->setForeground(QColor(m_theme == Theme::Dark ? "#9ba8be" : "#68768a"));
    m_lyricsList->addItem(item);
}

void MainWindow::refreshMediaDisplay(const MusicTrack &track)
{
    refreshVideoControls(track);
    syncVideoOutput();
}

void MainWindow::refreshVideoControls(const MusicTrack &track)
{
    const bool isVideo = track.isVideo && !track.filePath.isEmpty();
    m_videoControlsFrame->setVisible(isVideo);
    m_videoScaleCombo->setEnabled(isVideo);
    m_audioOnlyButton->setEnabled(isVideo);
    m_fullscreenButton->setEnabled(isVideo && !m_audioOnlyVideo);
}

void MainWindow::syncVideoOutput()
{
    const MusicTrack track = m_player.currentTrack();
    const bool showVideo = track.isVideo && !m_audioOnlyVideo;

    if (showVideo) {
        if (m_fullscreenWindow) {
            m_player.setVideoOutput(m_fullscreenWindow->videoWidget());
        } else {
            m_player.setVideoOutput(m_videoWidget);
        }
        m_mediaStack->setCurrentWidget(m_videoWidget);
        if (m_videoStateOverlay) {
            m_videoStateOverlay->raise();
        }
    } else {
        if (m_fullscreenWindow) {
            m_fullscreenWindow->close();
        }
        if (m_videoStateOverlay) {
            m_videoStateOverlay->hideIndicator();
        }
        m_player.setVideoOutput(nullptr);
        m_mediaStack->setCurrentWidget(m_coverLabel);
    }

    refreshVideoControls(track);
}

void MainWindow::applyVideoScale()
{
    const int mode = m_videoScaleCombo->currentData().toInt();
    Qt::AspectRatioMode aspectMode = Qt::KeepAspectRatio;
    if (mode == 1) {
        aspectMode = Qt::KeepAspectRatioByExpanding;
    } else if (mode == 2) {
        aspectMode = Qt::IgnoreAspectRatio;
    }

    m_videoWidget->setAspectRatioMode(aspectMode);
    if (m_fullscreenWindow) {
        m_fullscreenWindow->setAspectRatioMode(aspectMode);
    }
}

void MainWindow::toggleVideoFullscreen()
{
    const MusicTrack track = m_player.currentTrack();
    if (!track.isVideo || m_audioOnlyVideo) {
        return;
    }

    if (m_fullscreenWindow) {
        m_fullscreenWindow->close();
        return;
    }

    m_fullscreenWindow = new FullscreenVideoWindow(&m_player, m_language == Language::English, this);
    m_fullscreenWindow->setAttribute(Qt::WA_DeleteOnClose);
    m_fullscreenWindow->setAspectRatioMode(m_videoWidget->aspectRatioMode());
    connect(m_fullscreenWindow, &FullscreenVideoWindow::closed, this, [this] {
        m_fullscreenWindow = nullptr;
        if (m_player.currentTrack().isVideo && !m_audioOnlyVideo) {
            m_player.setVideoOutput(m_videoWidget);
            m_mediaStack->setCurrentWidget(m_videoWidget);
        }
    });

    m_player.setVideoOutput(m_fullscreenWindow->videoWidget());
    m_fullscreenWindow->showFullScreen();
    m_fullscreenWindow->raise();
    m_fullscreenWindow->activateWindow();
}

void MainWindow::exitVideoFullscreen()
{
    if (m_fullscreenWindow) {
        m_fullscreenWindow->close();
    }
}

bool MainWindow::handleVideoKeyEvent(QKeyEvent *event, bool pressed)
{
    if (!event) {
        return false;
    }

    const int key = event->key();
    if (key == Qt::Key_Escape && pressed) {
        exitVideoFullscreen();
        return true;
    }

    if (key == Qt::Key_Space && pressed && !event->isAutoRepeat()) {
        m_player.togglePlayPause();
        return true;
    }

    if (key == Qt::Key_Left && pressed && !event->isAutoRepeat()) {
        seekRelative(-5000);
        return true;
    }

    if (key == Qt::Key_Right) {
        if (pressed) {
            if (!event->isAutoRepeat() && !m_rightKeyPressed) {
                m_rightKeyPressed = true;
                m_rightLongPressActive = false;
                m_rightHoldTimer->start(450);
            }
            return true;
        }

        if (event->isAutoRepeat()) {
            return true;
        }

        m_rightKeyPressed = false;
        if (m_rightLongPressActive) {
            m_rightLongPressActive = false;
            m_player.setPlaybackRate(1.0);
            setStatus(text(TextKey::Playing).arg(m_player.currentTrack().displayTitle()));
        } else {
            m_rightHoldTimer->stop();
            seekRelative(5000);
        }
        return true;
    }

    return false;
}

void MainWindow::seekRelative(qint64 deltaMs)
{
    const qint64 current = m_positionSlider ? m_positionSlider->value() : 0;
    const qint64 maximum = m_currentDurationMs > 0 ? m_currentDurationMs : (m_positionSlider ? m_positionSlider->maximum() : 0);
    const qint64 target = qBound<qint64>(0, current + deltaMs, maximum);
    m_player.seek(target);
    if (m_positionSlider) {
        m_positionSlider->setValue(static_cast<int>(target));
    }
    updatePositionLabel(target, m_currentDurationMs);
}

void MainWindow::setCoverFromImage(const QImage &image)
{
    if (!image.isNull()) {
        setCoverFromPixmap(QPixmap::fromImage(image));
    }
}

void MainWindow::setCoverFromPixmap(const QPixmap &pixmap)
{
    QPixmap source = pixmap.isNull() ? defaultCoverPixmap() : pixmap;
    const QSize targetSize(CoverSize, CoverSize);
    QPixmap scaled = source.scaled(targetSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);

    const QRect cropRect((scaled.width() - targetSize.width()) / 2,
                         (scaled.height() - targetSize.height()) / 2,
                         targetSize.width(),
                         targetSize.height());
    scaled = scaled.copy(cropRect);

    QPixmap rounded(targetSize);
    rounded.fill(Qt::transparent);

    QPainter painter(&rounded);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addRoundedRect(QRectF(QPointF(0, 0), QSizeF(targetSize)), 22, 22);
    painter.setClipPath(path);
    painter.drawPixmap(0, 0, scaled);
    painter.end();

    m_coverLabel->setPixmap(rounded);
}

void MainWindow::updateResponsiveLayout()
{
    const int windowWidth = width();
    const bool compactHeader = windowWidth < 780;
    const bool stackedContent = windowWidth < 980;

    if (m_headerLayout) {
        m_headerLayout->setDirection(compactHeader ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
        m_headerLayout->setSpacing(compactHeader ? 10 : 16);
    }

    if (m_contentLayout && m_libraryPanel && m_nowPlayingPanel) {
        m_contentLayout->setDirection(stackedContent ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
        m_contentLayout->setStretchFactor(m_libraryPanel, stackedContent ? 1 : 3);
        m_contentLayout->setStretchFactor(m_nowPlayingPanel, stackedContent ? 1 : 2);
    }

    if (m_mediaStack) {
        m_mediaStack->setMinimumSize(stackedContent ? QSize(220, 150) : QSize(240, 180));
    }

    if (m_libraryTable) {
        m_libraryTable->setColumnWidth(PathColumn, stackedContent ? 150 : 180);
    }
}

QString MainWindow::selectedFolderFilter() const
{
    if (!m_folderFilterCombo) {
        return {};
    }

    return m_folderFilterCombo->currentData().toString();
}

MainWindow::LibraryView MainWindow::selectedLibraryView() const
{
    if (!m_libraryViewCombo) {
        return LibraryView::List;
    }

    return m_libraryViewCombo->currentData().toString() == QStringLiteral("icons") ? LibraryView::Icons : LibraryView::List;
}

MainWindow::IconSize MainWindow::selectedIconSize() const
{
    if (!m_iconSizeCombo) {
        return IconSize::Medium;
    }

    const QString value = m_iconSizeCombo->currentData().toString();
    if (value == QStringLiteral("small")) {
        return IconSize::Small;
    }
    if (value == QStringLiteral("large")) {
        return IconSize::Large;
    }

    return IconSize::Medium;
}

bool MainWindow::trackMatchesFolderFilter(const MusicTrack &track, const QString &folderPath) const
{
    if (folderPath.isEmpty()) {
        return true;
    }

    QString normalizedFolder = QDir::toNativeSeparators(QFileInfo(folderPath).absoluteFilePath());
    QString normalizedTrackPath = QDir::toNativeSeparators(QFileInfo(track.filePath).absoluteFilePath());

    if (normalizedFolder.endsWith(QDir::separator()) && !QDir(normalizedFolder).isRoot()) {
        normalizedFolder.chop(1);
    }

    const QString childPrefix = normalizedFolder + QDir::separator();
    return normalizedTrackPath.compare(normalizedFolder, Qt::CaseInsensitive) == 0
        || normalizedTrackPath.startsWith(childPrefix, Qt::CaseInsensitive);
}

QIcon MainWindow::iconForTrack(const MusicTrack &track)
{
    const QString key = artworkKey(track);
    QPixmap pixmap = m_artworkCache.value(key);
    if (pixmap.isNull()) {
        requestArtwork(track);
        pixmap = defaultCoverPixmap();
    }

    const QSize targetSize = selectedIconExtent();
    QPixmap scaled = pixmap.scaled(targetSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    const QRect cropRect((scaled.width() - targetSize.width()) / 2,
                         (scaled.height() - targetSize.height()) / 2,
                         targetSize.width(),
                         targetSize.height());
    scaled = scaled.copy(cropRect);

    QPixmap rounded(targetSize);
    rounded.fill(Qt::transparent);

    QPainter painter(&rounded);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addRoundedRect(QRectF(QPointF(0, 0), QSizeF(targetSize)), 16, 16);
    painter.setClipPath(path);
    painter.drawPixmap(0, 0, scaled);

    if (track.isVideo) {
        painter.setClipping(false);
        painter.setBrush(QColor(0, 0, 0, 150));
        painter.setPen(Qt::NoPen);
        const int badgeSize = qMax(24, targetSize.width() / 3);
        const QRect badgeRect(targetSize.width() - badgeSize - 8, targetSize.height() - badgeSize - 8, badgeSize, badgeSize);
        painter.drawEllipse(badgeRect);
        QPainterPath playPath;
        const QPointF center = badgeRect.center();
        const qreal side = badgeSize * 0.34;
        playPath.moveTo(center.x() - side * 0.45, center.y() - side);
        playPath.lineTo(center.x() - side * 0.45, center.y() + side);
        playPath.lineTo(center.x() + side * 0.9, center.y());
        playPath.closeSubpath();
        painter.setBrush(QColor("#ffffff"));
        painter.drawPath(playPath);
    }

    return QIcon(rounded);
}

QSize MainWindow::selectedIconExtent() const
{
    switch (selectedIconSize()) {
    case IconSize::Small:
        return QSize(72, 72);
    case IconSize::Large:
        return QSize(148, 148);
    case IconSize::Medium:
        return QSize(108, 108);
    }

    return QSize(108, 108);
}

QString MainWindow::artworkKey(const MusicTrack &track) const
{
    if (track.filePath.isEmpty()) {
        return {};
    }

    return QFileInfo(track.filePath).absoluteFilePath();
}

void MainWindow::playTrackAt(int index)
{
    if (index < 0 || index >= m_filteredTracks.size()) {
        return;
    }

    m_player.setQueue(m_filteredTracks, index);
    m_library.saveQueue(m_filteredTracks);
    m_player.playAt(index);
}

int MainWindow::iconItemToTrackIndex(QListWidgetItem *item) const
{
    if (!item) {
        return -1;
    }

    const int index = item->data(TrackIndexRole).toInt();
    return index >= 0 && index < m_filteredTracks.size() ? index : -1;
}

void MainWindow::showLibraryContextMenu(const QPoint &position, bool iconView)
{
    const int index = iconView
                          ? iconItemToTrackIndex(m_libraryIconList->itemAt(position))
                          : rowToTrackIndex(m_libraryTable->rowAt(position.y()));
    if (index < 0) {
        return;
    }

    QMenu menu(this);
    QAction *openAction = menu.addAction(text(TextKey::OpenInFolder));
    QWidget *sourceWidget = iconView ? static_cast<QWidget *>(m_libraryIconList) : static_cast<QWidget *>(m_libraryTable);
    QAction *selectedAction = menu.exec(sourceWidget->mapToGlobal(position));
    if (selectedAction == openAction) {
        openTrackInFolder(index);
    }
}

void MainWindow::openTrackInFolder(int index) const
{
    if (index < 0 || index >= m_filteredTracks.size()) {
        return;
    }

    const QFileInfo fileInfo(m_filteredTracks.at(index).filePath);

    if (fileInfo.exists()) {
        const QString nativeFilePath = QDir::toNativeSeparators(fileInfo.canonicalFilePath());
        QProcess::startDetached(QStringLiteral("explorer.exe"), QStringList{QStringLiteral("/select,"), nativeFilePath});
        return;
    }

    if (QDir(fileInfo.absolutePath()).exists()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(fileInfo.absolutePath()));
    }
}

QPixmap MainWindow::coverFromSidecarFile(const MusicTrack &track) const
{
    const QFileInfo info(track.filePath);
    const QString dirPath = info.absolutePath();
    const QString basePath = dirPath + "/" + info.completeBaseName();
    const QStringList candidates = {
        basePath + ".jpg",
        basePath + ".jpeg",
        basePath + ".png",
        basePath + ".webp",
        dirPath + "/cover.jpg",
        dirPath + "/cover.png",
        dirPath + "/folder.jpg",
        dirPath + "/folder.png",
        dirPath + "/album.jpg",
        dirPath + "/album.png"
    };

    for (const QString &candidate : candidates) {
        QPixmap pixmap;
        if (QFileInfo::exists(candidate) && pixmap.load(candidate)) {
            return pixmap;
        }
    }

    return {};
}

QPixmap MainWindow::defaultCoverPixmap() const
{
    return QPixmap(":/images/default-cover.png");
}

QString MainWindow::text(TextKey key) const
{
    const bool english = m_language == Language::English;

    switch (key) {
    case TextKey::WindowTitle:
        return english ? QStringLiteral("MusicPlayer") : QStringLiteral("本地媒体播放器");
    case TextKey::AppSubtitle:
        return english ? QStringLiteral("Local music and video, cover art, synced lyrics")
                       : QStringLiteral("本地音乐与视频、封面展示、歌词同步");
    case TextKey::AddFolder:
        return english ? QStringLiteral("Add Folder") : QStringLiteral("添加文件夹");
    case TextKey::ShowFolders:
        return english ? QStringLiteral("Folders") : QStringLiteral("已添加文件夹");
    case TextKey::Rescan:
        return english ? QStringLiteral("Rescan") : QStringLiteral("重新扫描");
    case TextKey::AllFolders:
        return english ? QStringLiteral("All folders") : QStringLiteral("全部文件夹");
    case TextKey::SearchPlaceholder:
        return english ? QStringLiteral("Search local title, artist, album, type, or path")
                       : QStringLiteral("搜索本地标题、歌手、专辑、类型或路径");
    case TextKey::LocalMusic:
        return english ? QStringLiteral("Local Library") : QStringLiteral("本地媒体库");
    case TextKey::Settings:
        return english ? QStringLiteral("Settings") : QStringLiteral("设置");
    case TextKey::Language:
        return english ? QStringLiteral("Language") : QStringLiteral("语言");
    case TextKey::Theme:
        return english ? QStringLiteral("Theme") : QStringLiteral("主题");
    case TextKey::SwitchToLight:
        return english ? QStringLiteral("Light") : QStringLiteral("浅色");
    case TextKey::SwitchToDark:
        return english ? QStringLiteral("Dark") : QStringLiteral("深色");
    case TextKey::ViewMode:
        return english ? QStringLiteral("View") : QStringLiteral("视图");
    case TextKey::ListView:
        return english ? QStringLiteral("List") : QStringLiteral("列表");
    case TextKey::IconView:
        return english ? QStringLiteral("Icons") : QStringLiteral("图标");
    case TextKey::IconSizeLabel:
        return english ? QStringLiteral("Icon Size") : QStringLiteral("图标大小");
    case TextKey::SmallIcons:
        return english ? QStringLiteral("Small") : QStringLiteral("小图标");
    case TextKey::MediumIcons:
        return english ? QStringLiteral("Medium") : QStringLiteral("中图标");
    case TextKey::LargeIcons:
        return english ? QStringLiteral("Large") : QStringLiteral("大图标");
    case TextKey::OpenInFolder:
        return english ? QStringLiteral("Open in local folder") : QStringLiteral("在本地文件夹打开");
    case TextKey::Playback:
        return english ? QStringLiteral("Playback") : QStringLiteral("播放控制");
    case TextKey::Lyrics:
        return english ? QStringLiteral("Lyrics") : QStringLiteral("歌词");
    case TextKey::NoTrackSelected:
        return english ? QStringLiteral("No media selected") : QStringLiteral("未选择媒体");
    case TextKey::NoArtist:
        return english ? QStringLiteral("Unknown artist") : QStringLiteral("未知歌手");
    case TextKey::NoLyrics:
        return english ? QStringLiteral("No synced lyrics found. Put a .lrc file beside the media with the same name.")
                       : QStringLiteral("未找到同步歌词。请将同名 .lrc 文件放在媒体文件旁边。");
    case TextKey::Ready:
        return english ? QStringLiteral("Ready") : QStringLiteral("就绪");
    case TextKey::Previous:
        return english ? QStringLiteral("Previous") : QStringLiteral("上一首");
    case TextKey::Play:
        return english ? QStringLiteral("Play") : QStringLiteral("播放");
    case TextKey::Pause:
        return english ? QStringLiteral("Pause") : QStringLiteral("暂停");
    case TextKey::Next:
        return english ? QStringLiteral("Next") : QStringLiteral("下一首");
    case TextKey::Mode:
        return english ? QStringLiteral("Mode") : QStringLiteral("模式");
    case TextKey::Volume:
        return english ? QStringLiteral("Volume") : QStringLiteral("音量");
    case TextKey::Sequential:
        return english ? QStringLiteral("Sequential") : QStringLiteral("顺序播放");
    case TextKey::LoopAll:
        return english ? QStringLiteral("Loop All") : QStringLiteral("列表循环");
    case TextKey::LoopOne:
        return english ? QStringLiteral("Loop One") : QStringLiteral("单曲循环");
    case TextKey::Shuffle:
        return english ? QStringLiteral("Shuffle") : QStringLiteral("随机播放");
    case TextKey::Favorite:
        return english ? QStringLiteral("Fav") : QStringLiteral("收藏");
    case TextKey::Type:
        return english ? QStringLiteral("Type") : QStringLiteral("类型");
    case TextKey::Title:
        return english ? QStringLiteral("Title") : QStringLiteral("标题");
    case TextKey::Artist:
        return english ? QStringLiteral("Artist") : QStringLiteral("歌手");
    case TextKey::Album:
        return english ? QStringLiteral("Album") : QStringLiteral("专辑");
    case TextKey::Length:
        return english ? QStringLiteral("Length") : QStringLiteral("时长");
    case TextKey::Path:
        return english ? QStringLiteral("Path") : QStringLiteral("路径");
    case TextKey::Audio:
        return english ? QStringLiteral("Audio") : QStringLiteral("音频");
    case TextKey::Video:
        return english ? QStringLiteral("Video") : QStringLiteral("视频");
    case TextKey::Unknown:
        return english ? QStringLiteral("Unknown") : QStringLiteral("未知");
    case TextKey::Yes:
        return english ? QStringLiteral("Yes") : QStringLiteral("是");
    case TextKey::AddMusicFolder:
        return english ? QStringLiteral("Add Media Folder") : QStringLiteral("添加媒体文件夹");
    case TextKey::AddedFolders:
        return english ? QStringLiteral("Added folders") : QStringLiteral("已添加的文件夹");
    case TextKey::NoFolders:
        return english ? QStringLiteral("No folders have been added yet.") : QStringLiteral("还没有添加任何文件夹。");
    case TextKey::PlaybackProblem:
        return english ? QStringLiteral("Playback problem") : QStringLiteral("播放问题");
    case TextKey::LibraryError:
        return english ? QStringLiteral("Library error") : QStringLiteral("媒体库错误");
    case TextKey::LibraryOpenFailed:
        return english ? QStringLiteral("Library failed to open: %1") : QStringLiteral("媒体库打开失败：%1");
    case TextKey::FolderScanned:
        return english ? QStringLiteral("Folder scanned. %1 file(s) added or updated.")
                       : QStringLiteral("文件夹扫描完成，已添加或更新 %1 个文件。");
    case TextKey::RescanComplete:
        return english ? QStringLiteral("Rescan complete. %1 file(s) added or updated.")
                       : QStringLiteral("重新扫描完成，已添加或更新 %1 个文件。");
    case TextKey::Playing:
        return english ? QStringLiteral("Playing %1") : QStringLiteral("正在播放：%1");
    case TextKey::NowPlaying:
        return english ? QStringLiteral("Now playing: %1") : QStringLiteral("正在播放：%1");
    case TextKey::LocalTracks:
        return english ? QStringLiteral("%1 local item(s)") : QStringLiteral("本地媒体：%1 个");
    case TextKey::ShortcutHint:
        return english ? QStringLiteral("Space: play/pause  |  Ctrl+Left/Right: previous/next")
                       : QStringLiteral("空格：播放/暂停  |  Ctrl+左右：上一首/下一首");
    case TextKey::VideoControls:
        return english ? QStringLiteral("Video") : QStringLiteral("视频");
    case TextKey::VideoScale:
        return english ? QStringLiteral("Scale") : QStringLiteral("缩放");
    case TextKey::Fit:
        return english ? QStringLiteral("Fit") : QStringLiteral("适应");
    case TextKey::Fill:
        return english ? QStringLiteral("Fill") : QStringLiteral("填充");
    case TextKey::Stretch:
        return english ? QStringLiteral("Stretch") : QStringLiteral("拉伸");
    case TextKey::Fullscreen:
        return english ? QStringLiteral("Fullscreen") : QStringLiteral("全屏");
    case TextKey::AudioOnly:
        return english ? QStringLiteral("Audio Only") : QStringLiteral("仅音频");
    case TextKey::ShowVideo:
        return english ? QStringLiteral("Show Video") : QStringLiteral("显示视频");
    case TextKey::Chinese:
        return english ? QStringLiteral("Chinese") : QStringLiteral("中文");
    case TextKey::English:
        return english ? QStringLiteral("English") : QStringLiteral("英文");
    }

    return QString();
}

QString MainWindow::languageCode() const
{
    return m_language == Language::English ? QStringLiteral("en") : QStringLiteral("zh");
}

MainWindow::Language MainWindow::languageFromCode(const QString &code) const
{
    return code.compare("en", Qt::CaseInsensitive) == 0 ? Language::English : Language::Chinese;
}

QString MainWindow::themeCode() const
{
    return m_theme == Theme::Light ? QStringLiteral("light") : QStringLiteral("dark");
}

MainWindow::Theme MainWindow::themeFromCode(const QString &code) const
{
    return code.compare("light", Qt::CaseInsensitive) == 0 ? Theme::Light : Theme::Dark;
}

int MainWindow::rowToTrackIndex(int row) const
{
    if (row < 0 || row >= m_filteredTracks.size()) {
        return -1;
    }
    return row;
}

QString MainWindow::formatDuration(qint64 durationMs) const
{
    if (durationMs <= 0) {
        return "00:00";
    }

    const qint64 totalSeconds = durationMs / 1000;
    const qint64 hours = totalSeconds / 3600;
    const qint64 minutes = (totalSeconds % 3600) / 60;
    const qint64 seconds = totalSeconds % 60;

    if (hours > 0) {
        return QString("%1:%2:%3")
            .arg(hours)
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'));
    }

    return QString("%1:%2")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
}
