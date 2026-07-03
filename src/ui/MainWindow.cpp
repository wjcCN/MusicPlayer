#include "ui/MainWindow.h"

#include <QCloseEvent>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace
{
constexpr int FavoriteColumn = 0;
constexpr int TitleColumn = 1;
constexpr int ArtistColumn = 2;
constexpr int AlbumColumn = 3;
constexpr int DurationColumn = 4;
constexpr int PathColumn = 5;
}

MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent)
{
    buildUi();
    connectSignals();
    loadInitialState();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    savePlaybackState();
    QWidget::closeEvent(event);
}

void MainWindow::buildUi()
{
    setWindowTitle(text(TextKey::WindowTitle));
    resize(1180, 720);

    m_addFolderButton = new QPushButton(this);
    m_rescanButton = new QPushButton(this);
    m_searchEdit = new QLineEdit(this);

    m_libraryTitleLabel = new QLabel(this);
    m_libraryTitleLabel->setObjectName("sectionTitle");

    m_libraryTable = new QTableWidget(this);
    m_libraryTable->setColumnCount(6);
    m_libraryTable->verticalHeader()->setVisible(false);
    m_libraryTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_libraryTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_libraryTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_libraryTable->setAlternatingRowColors(true);
    m_libraryTable->horizontalHeader()->setStretchLastSection(true);
    m_libraryTable->horizontalHeader()->setSectionResizeMode(TitleColumn, QHeaderView::Stretch);
    m_libraryTable->horizontalHeader()->setSectionResizeMode(PathColumn, QHeaderView::ResizeToContents);
    m_libraryTable->setColumnWidth(FavoriteColumn, 52);
    m_libraryTable->setColumnWidth(DurationColumn, 88);

    m_platformTitleLabel = new QLabel(this);
    m_platformTitleLabel->setObjectName("sectionTitle");

    m_platformTable = new QTableWidget(this);
    m_platformTable->setColumnCount(3);
    m_platformTable->verticalHeader()->setVisible(false);
    m_platformTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_platformTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_platformTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_platformTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_platformTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_platformTable->setMaximumHeight(150);

    m_languageLabel = new QLabel(this);
    m_languageCombo = new QComboBox(this);
    m_languageCombo->addItem(QString(), QStringLiteral("zh"));
    m_languageCombo->addItem(QString(), QStringLiteral("en"));

    auto *settingsLayout = new QHBoxLayout;
    settingsLayout->addWidget(m_languageLabel);
    settingsLayout->addWidget(m_languageCombo);
    settingsLayout->addStretch();

    m_settingsBox = new QGroupBox(this);
    m_settingsBox->setLayout(settingsLayout);

    m_nowPlayingLabel = new QLabel(this);
    m_nowPlayingLabel->setObjectName("nowPlaying");
    m_timeLabel = new QLabel("00:00 / 00:00", this);
    m_statusLabel = new QLabel(this);

    m_previousButton = new QPushButton(this);
    m_playPauseButton = new QPushButton(this);
    m_nextButton = new QPushButton(this);

    m_positionSlider = new QSlider(Qt::Horizontal, this);
    m_positionSlider->setRange(0, 0);
    m_volumeSlider = new QSlider(Qt::Horizontal, this);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(70);
    m_volumeSlider->setMaximumWidth(160);

    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem(QString(), static_cast<int>(PlaybackMode::Sequential));
    m_modeCombo->addItem(QString(), static_cast<int>(PlaybackMode::LoopAll));
    m_modeCombo->addItem(QString(), static_cast<int>(PlaybackMode::LoopOne));
    m_modeCombo->addItem(QString(), static_cast<int>(PlaybackMode::Shuffle));

    auto *topControls = new QHBoxLayout;
    topControls->addWidget(m_addFolderButton);
    topControls->addWidget(m_rescanButton);
    topControls->addSpacing(12);
    topControls->addWidget(m_searchEdit, 1);

    auto *libraryLayout = new QVBoxLayout;
    libraryLayout->addWidget(m_libraryTitleLabel);
    libraryLayout->addLayout(topControls);
    libraryLayout->addWidget(m_libraryTable, 1);
    libraryLayout->addWidget(m_settingsBox);
    libraryLayout->addWidget(m_platformTitleLabel);
    libraryLayout->addWidget(m_platformTable);

    auto *playbackButtons = new QHBoxLayout;
    playbackButtons->addWidget(m_previousButton);
    playbackButtons->addWidget(m_playPauseButton);
    playbackButtons->addWidget(m_nextButton);
    playbackButtons->addStretch();
    m_modeLabel = new QLabel(this);
    playbackButtons->addWidget(m_modeLabel);
    playbackButtons->addWidget(m_modeCombo);
    playbackButtons->addSpacing(16);
    m_volumeLabel = new QLabel(this);
    playbackButtons->addWidget(m_volumeLabel);
    playbackButtons->addWidget(m_volumeSlider);

    auto *playerLayout = new QVBoxLayout;
    playerLayout->addWidget(m_nowPlayingLabel);
    playerLayout->addWidget(m_positionSlider);

    auto *timeStatusLayout = new QHBoxLayout;
    timeStatusLayout->addWidget(m_timeLabel);
    timeStatusLayout->addStretch();
    timeStatusLayout->addWidget(m_statusLabel);
    playerLayout->addLayout(timeStatusLayout);
    playerLayout->addLayout(playbackButtons);

    m_playerBox = new QGroupBox(this);
    m_playerBox->setLayout(playerLayout);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(libraryLayout, 1);
    mainLayout->addWidget(m_playerBox);
    setLayout(mainLayout);

    applyLanguage();
}

void MainWindow::connectSignals()
{
    connect(m_addFolderButton, &QPushButton::clicked, this, [this] {
        const QString folder = QFileDialog::getExistingDirectory(this, text(TextKey::AddMusicFolder));
        if (folder.isEmpty()) {
            return;
        }

        const int changed = m_library.addFolder(folder);
        refreshLibraryTable();
        if (m_player.queue().isEmpty() && !m_allTracks.isEmpty()) {
            m_player.setQueue(m_allTracks);
        }
        setStatus(text(TextKey::FolderScanned).arg(changed));
    });

    connect(m_rescanButton, &QPushButton::clicked, this, [this] {
        const int changed = m_library.rescanFolders();
        refreshLibraryTable();
        if (m_player.queue().isEmpty() && !m_allTracks.isEmpty()) {
            m_player.setQueue(m_allTracks);
        }
        setStatus(text(TextKey::RescanComplete).arg(changed));
    });

    connect(m_searchEdit, &QLineEdit::textChanged, this, [this] {
        refreshLibraryTable();
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

        m_player.setQueue(m_filteredTracks, index);
        m_library.saveQueue(m_filteredTracks);
        m_player.playAt(index);
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

    connect(m_positionSlider, &QSlider::sliderPressed, this, [this] {
        m_seeking = true;
    });
    connect(m_positionSlider, &QSlider::sliderReleased, this, [this] {
        m_seeking = false;
        m_player.seek(m_positionSlider->value());
    });

    connect(&m_player, &PlaybackController::currentTrackChanged, this, [this](const MusicTrack &track) {
        m_nowPlayingLabel->setText(text(TextKey::NowPlaying).arg(track.displayTitle()));
        m_library.markPlayed(track.filePath);
        setStatus(text(TextKey::Playing).arg(track.displayTitle()));
    });
    connect(&m_player, &PlaybackController::playbackStateChanged, this, &MainWindow::applyPlaybackState);
    connect(&m_player, &PlaybackController::positionChanged, this, [this](qint64 position) {
        if (!m_seeking) {
            m_positionSlider->setValue(static_cast<int>(position));
        }
        updatePositionLabel(position, m_currentDurationMs);
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

void MainWindow::loadInitialState()
{
    if (!m_library.open()) {
        setStatus(text(TextKey::LibraryOpenFailed).arg(m_library.lastError()));
        QMessageBox::critical(this, text(TextKey::LibraryError), m_library.lastError());
        return;
    }

    m_libraryReady = true;
    setLanguage(languageFromCode(m_library.setting("language", "zh")));

    const int volume = m_library.setting("volume", "70").toInt();
    m_volumeSlider->setValue(qBound(0, volume, 100));
    m_player.setVolume(m_volumeSlider->value());

    const int modeValue = m_library.setting("playback_mode", QString::number(static_cast<int>(PlaybackMode::Sequential))).toInt();
    const int modeIndex = m_modeCombo->findData(modeValue);
    if (modeIndex >= 0) {
        m_modeCombo->setCurrentIndex(modeIndex);
    }

    refreshLibraryTable();
    refreshPlatformTable();

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
    m_addFolderButton->setText(text(TextKey::AddFolder));
    m_rescanButton->setText(text(TextKey::Rescan));
    m_searchEdit->setPlaceholderText(text(TextKey::SearchPlaceholder));
    m_libraryTitleLabel->setText(text(TextKey::LocalMusic));
    m_platformTitleLabel->setText(text(TextKey::OfficialPlatformAccess));
    m_settingsBox->setTitle(text(TextKey::Settings));
    m_languageLabel->setText(text(TextKey::Language));
    m_playerBox->setTitle(text(TextKey::Playback));
    m_modeLabel->setText(text(TextKey::Mode));
    m_volumeLabel->setText(text(TextKey::Volume));
    m_previousButton->setText(text(TextKey::Previous));
    m_nextButton->setText(text(TextKey::Next));

    m_libraryTable->setHorizontalHeaderLabels({
        text(TextKey::Favorite),
        text(TextKey::Title),
        text(TextKey::Artist),
        text(TextKey::Album),
        text(TextKey::Length),
        text(TextKey::Path)
    });
    m_platformTable->setHorizontalHeaderLabels({
        text(TextKey::Platform),
        text(TextKey::Status),
        text(TextKey::Requirement)
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

    const MusicTrack currentTrack = m_player.currentTrack();
    if (currentTrack.filePath.isEmpty()) {
        m_nowPlayingLabel->setText(text(TextKey::NoTrackSelected));
    } else {
        m_nowPlayingLabel->setText(text(TextKey::NowPlaying).arg(currentTrack.displayTitle()));
    }

    applyPlaybackState(m_player.playbackState());
    refreshPlatformTable();
    if (m_libraryReady) {
        refreshLibraryTable();
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
    m_library.setSetting("language", languageCode());
    applyLanguage();
}

void MainWindow::refreshLibraryTable()
{
    m_allTracks = m_library.tracks();
    const QString filter = m_searchEdit->text().trimmed();
    m_filteredTracks.clear();

    for (const MusicTrack &track : m_allTracks) {
        const QString haystack = QString("%1 %2 %3 %4").arg(track.title, track.artist, track.album, track.filePath);
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
        m_libraryTable->setItem(row, TitleColumn, new QTableWidgetItem(track.displayTitle()));
        m_libraryTable->setItem(row, ArtistColumn, new QTableWidgetItem(track.artist.isEmpty() ? text(TextKey::Unknown) : track.artist));
        m_libraryTable->setItem(row, AlbumColumn, new QTableWidgetItem(track.album.isEmpty() ? text(TextKey::Unknown) : track.album));
        m_libraryTable->setItem(row, DurationColumn, new QTableWidgetItem(formatDuration(track.durationMs)));
        m_libraryTable->setItem(row, PathColumn, new QTableWidgetItem(QDir::toNativeSeparators(fileInfo.absoluteFilePath())));

        if (!fileInfo.exists()) {
            for (int column = 0; column < m_libraryTable->columnCount(); ++column) {
                if (auto *item = m_libraryTable->item(row, column)) {
                    item->setForeground(Qt::gray);
                }
            }
        }
    }

    setStatus(text(TextKey::LocalTracks).arg(m_filteredTracks.size()));
}

void MainWindow::refreshPlatformTable()
{
    const QList<ProviderInfo> providers = m_providerRegistry.providers();
    m_platformTable->setRowCount(providers.size());

    for (int row = 0; row < providers.size(); ++row) {
        const ProviderInfo &provider = providers.at(row);
        TextKey platformNameKey = TextKey::QqMusic;
        if (provider.id == "netease") {
            platformNameKey = TextKey::NeteaseMusic;
        } else if (provider.id == "kugou") {
            platformNameKey = TextKey::KugouMusic;
        }
        m_platformTable->setItem(row, 0, new QTableWidgetItem(text(platformNameKey)));
        m_platformTable->setItem(row, 1, new QTableWidgetItem(text(TextKey::NotConfigured)));

        TextKey requirementKey = TextKey::QqMusicRequirement;
        if (provider.id == "netease") {
            requirementKey = TextKey::NeteaseRequirement;
        } else if (provider.id == "kugou") {
            requirementKey = TextKey::KugouRequirement;
        }
        m_platformTable->setItem(row, 2, new QTableWidgetItem(text(requirementKey)));
    }
}

void MainWindow::setStatus(const QString &message)
{
    m_statusLabel->setText(message);
}

void MainWindow::applyPlaybackState(QMediaPlayer::PlaybackState state)
{
    m_playPauseButton->setText(state == QMediaPlayer::PlayingState ? text(TextKey::Pause) : text(TextKey::Play));
}

void MainWindow::updatePositionLabel(qint64 positionMs, qint64 durationMs)
{
    m_timeLabel->setText(QString("%1 / %2").arg(formatDuration(positionMs), formatDuration(durationMs)));
}

void MainWindow::savePlaybackState()
{
    m_library.saveQueue(m_player.queue());
    m_library.setSetting("volume", QString::number(m_volumeSlider->value()));
    m_library.setSetting("playback_mode", QString::number(static_cast<int>(m_player.playbackMode())));
    m_library.setSetting("language", languageCode());
}

QString MainWindow::text(TextKey key) const
{
    const bool english = m_language == Language::English;

    switch (key) {
    case TextKey::WindowTitle:
        return english ? QStringLiteral("MusicPlayer") : QStringLiteral("音乐播放器");
    case TextKey::AddFolder:
        return english ? QStringLiteral("Add Folder") : QStringLiteral("添加文件夹");
    case TextKey::Rescan:
        return english ? QStringLiteral("Rescan") : QStringLiteral("重新扫描");
    case TextKey::SearchPlaceholder:
        return english ? QStringLiteral("Search title, artist, album, or path") : QStringLiteral("搜索标题、歌手、专辑或路径");
    case TextKey::LocalMusic:
        return english ? QStringLiteral("Local Music") : QStringLiteral("本地音乐");
    case TextKey::OfficialPlatformAccess:
        return english ? QStringLiteral("Official Platform Access") : QStringLiteral("官方平台接入");
    case TextKey::Settings:
        return english ? QStringLiteral("Settings") : QStringLiteral("设置");
    case TextKey::Language:
        return english ? QStringLiteral("Language") : QStringLiteral("语言");
    case TextKey::Playback:
        return english ? QStringLiteral("Playback") : QStringLiteral("播放控制");
    case TextKey::NoTrackSelected:
        return english ? QStringLiteral("No track selected") : QStringLiteral("未选择歌曲");
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
    case TextKey::Platform:
        return english ? QStringLiteral("Platform") : QStringLiteral("平台");
    case TextKey::Status:
        return english ? QStringLiteral("Status") : QStringLiteral("状态");
    case TextKey::Requirement:
        return english ? QStringLiteral("Requirement") : QStringLiteral("接入要求");
    case TextKey::Unknown:
        return english ? QStringLiteral("Unknown") : QStringLiteral("未知");
    case TextKey::Yes:
        return english ? QStringLiteral("Yes") : QStringLiteral("是");
    case TextKey::AddMusicFolder:
        return english ? QStringLiteral("Add Music Folder") : QStringLiteral("添加音乐文件夹");
    case TextKey::PlaybackProblem:
        return english ? QStringLiteral("Playback problem") : QStringLiteral("播放问题");
    case TextKey::LibraryError:
        return english ? QStringLiteral("Library error") : QStringLiteral("音乐库错误");
    case TextKey::LibraryOpenFailed:
        return english ? QStringLiteral("Library failed to open: %1") : QStringLiteral("音乐库打开失败：%1");
    case TextKey::FolderScanned:
        return english ? QStringLiteral("Folder scanned. %1 file(s) added or updated.") : QStringLiteral("文件夹扫描完成，已添加或更新 %1 个文件。");
    case TextKey::RescanComplete:
        return english ? QStringLiteral("Rescan complete. %1 file(s) added or updated.") : QStringLiteral("重新扫描完成，已添加或更新 %1 个文件。");
    case TextKey::Playing:
        return english ? QStringLiteral("Playing %1") : QStringLiteral("正在播放：%1");
    case TextKey::NowPlaying:
        return english ? QStringLiteral("Now playing: %1") : QStringLiteral("正在播放：%1");
    case TextKey::LocalTracks:
        return english ? QStringLiteral("%1 local track(s)") : QStringLiteral("本地音乐：%1 首");
    case TextKey::QqMusic:
        return english ? QStringLiteral("QQ Music") : QStringLiteral("QQ 音乐");
    case TextKey::NeteaseMusic:
        return english ? QStringLiteral("NetEase Cloud Music") : QStringLiteral("网易云音乐");
    case TextKey::KugouMusic:
        return english ? QStringLiteral("Kugou Music") : QStringLiteral("酷狗音乐");
    case TextKey::QqMusicRequirement:
        return english ? QStringLiteral("Requires an official authorization API or official client launch flow.")
                       : QStringLiteral("需要官方授权接口或官方客户端跳转方案。");
    case TextKey::NeteaseRequirement:
        return english ? QStringLiteral("Requires an official authorization API or official web/client launch flow.")
                       : QStringLiteral("需要官方授权接口或官方网页/客户端跳转方案。");
    case TextKey::KugouRequirement:
        return english ? QStringLiteral("Requires an official authorization API or TME-related authorization service.")
                       : QStringLiteral("需要官方授权接口或 TME 相关授权服务。");
    case TextKey::NotConfigured:
        return english ? QStringLiteral("Not configured") : QStringLiteral("未配置");
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
