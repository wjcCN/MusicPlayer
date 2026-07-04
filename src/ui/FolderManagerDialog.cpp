#include "ui/FolderManagerDialog.h"

#include "data/MusicLibrary.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

FolderManagerDialog::FolderManagerDialog(MusicLibrary *library, bool english, QWidget *parent)
    : QDialog(parent)
    , m_library(library)
    , m_english(english)
{
    setObjectName("folderManagerDialog");
    setWindowTitle(text("title"));
    resize(720, 460);

    auto *titleLabel = new QLabel(text("heading"), this);
    titleLabel->setObjectName("sectionTitle");

    m_folderList = new QListWidget(this);
    m_folderList->setObjectName("folderList");

    m_pathEdit = new QLineEdit(this);
    m_pathEdit->setObjectName("pathEdit");
    m_pathEdit->setPlaceholderText(text("placeholder"));

    auto *browseButton = new QPushButton(text("browse"), this);
    browseButton->setObjectName("ghostButton");
    auto *addButton = new QPushButton(text("add"), this);
    addButton->setObjectName("primaryButton");
    auto *removeButton = new QPushButton(text("remove"), this);
    removeButton->setObjectName("ghostButton");
    auto *rescanButton = new QPushButton(text("rescan"), this);
    rescanButton->setObjectName("ghostButton");
    auto *closeButton = new QPushButton(text("close"), this);
    closeButton->setObjectName("ghostButton");

    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName("statusLabel");
    m_statusLabel->setWordWrap(true);

    auto *pathLayout = new QHBoxLayout;
    pathLayout->setSpacing(10);
    pathLayout->addWidget(m_pathEdit, 1);
    pathLayout->addWidget(browseButton);
    pathLayout->addWidget(addButton);

    auto *actionLayout = new QHBoxLayout;
    actionLayout->setSpacing(10);
    actionLayout->addWidget(removeButton);
    actionLayout->addWidget(rescanButton);
    actionLayout->addStretch();
    actionLayout->addWidget(closeButton);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(12);
    layout->addWidget(titleLabel);
    layout->addWidget(m_folderList, 1);
    layout->addLayout(pathLayout);
    layout->addLayout(actionLayout);
    layout->addWidget(m_statusLabel);
    setLayout(layout);

    connect(browseButton, &QPushButton::clicked, this, &FolderManagerDialog::browseFolder);
    connect(addButton, &QPushButton::clicked, this, &FolderManagerDialog::addFolder);
    connect(removeButton, &QPushButton::clicked, this, &FolderManagerDialog::removeSelectedFolder);
    connect(rescanButton, &QPushButton::clicked, this, &FolderManagerDialog::rescanFolders);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_folderList, &QListWidget::currentTextChanged, this, [this](const QString &path) {
        if (!path.isEmpty()) {
            m_pathEdit->setText(path);
        }
    });

    refreshFolders();
}

QString FolderManagerDialog::text(const char *key) const
{
    const QString name = QString::fromLatin1(key);
    if (name == "title") {
        return m_english ? QStringLiteral("Media folders") : QStringLiteral("媒体文件夹");
    }
    if (name == "heading") {
        return m_english ? QStringLiteral("Loaded folders") : QStringLiteral("已载入文件夹");
    }
    if (name == "placeholder") {
        return m_english ? QStringLiteral("Input a local folder path, or browse to select one")
                         : QStringLiteral("输入本地文件夹路径，或点击浏览选择");
    }
    if (name == "browse") {
        return m_english ? QStringLiteral("Browse") : QStringLiteral("浏览");
    }
    if (name == "add") {
        return m_english ? QStringLiteral("Add") : QStringLiteral("添加");
    }
    if (name == "remove") {
        return m_english ? QStringLiteral("Remove Selected") : QStringLiteral("删除选中目录");
    }
    if (name == "rescan") {
        return m_english ? QStringLiteral("Rescan All") : QStringLiteral("重新扫描全部");
    }
    if (name == "close") {
        return m_english ? QStringLiteral("Close") : QStringLiteral("关闭");
    }
    if (name == "empty") {
        return m_english ? QStringLiteral("No folders loaded yet.") : QStringLiteral("还没有载入任何文件夹。");
    }
    if (name == "emptyPath") {
        return m_english ? QStringLiteral("Please input or choose a folder path.") : QStringLiteral("请先输入或选择文件夹路径。");
    }
    if (name == "missing") {
        return m_english ? QStringLiteral("The folder does not exist.") : QStringLiteral("该文件夹不存在。");
    }
    if (name == "duplicate") {
        return m_english ? QStringLiteral("This folder has already been added.") : QStringLiteral("该文件夹已经添加过。");
    }
    if (name == "added") {
        return m_english ? QStringLiteral("Added successfully. %1 media file(s) were scanned.")
                         : QStringLiteral("添加成功，已扫描到 %1 个媒体文件。");
    }
    if (name == "addFailed") {
        return m_english ? QStringLiteral("Add failed. Please check the path and try again.")
                         : QStringLiteral("添加失败，请检查路径后重试。");
    }
    if (name == "selectRemove") {
        return m_english ? QStringLiteral("Please select a folder to remove.") : QStringLiteral("请先选择要删除的目录。");
    }
    if (name == "confirmRemove") {
        return m_english ? QStringLiteral("Remove this folder from the library?\n%1")
                         : QStringLiteral("要从媒体库中移除这个目录吗？\n%1");
    }
    if (name == "removed") {
        return m_english ? QStringLiteral("Removed from the library.") : QStringLiteral("已从媒体库移除。");
    }
    if (name == "removeFailed") {
        return m_english ? QStringLiteral("Remove failed.") : QStringLiteral("删除失败。");
    }
    if (name == "rescanned") {
        return m_english ? QStringLiteral("Rescan complete. %1 media file(s) were added or updated.")
                         : QStringLiteral("重新扫描完成，已添加或更新 %1 个媒体文件。");
    }
    return {};
}

QString FolderManagerDialog::normalizedPath(const QString &path) const
{
    const QFileInfo info(path.trimmed());
    return QDir::toNativeSeparators(info.absoluteFilePath());
}

void FolderManagerDialog::refreshFolders()
{
    m_folderList->clear();
    const QStringList folders = m_library ? m_library->folders() : QStringList();
    for (const QString &folder : folders) {
        m_folderList->addItem(folder);
    }

    setStatus(folders.isEmpty() ? text("empty") : QString(), true);
}

void FolderManagerDialog::browseFolder()
{
    const QString folder = QFileDialog::getExistingDirectory(this, text("title"));
    if (!folder.isEmpty()) {
        m_pathEdit->setText(QDir::toNativeSeparators(folder));
    }
}

void FolderManagerDialog::addFolder()
{
    if (!m_library) {
        return;
    }

    const QString rawPath = m_pathEdit->text().trimmed();
    if (rawPath.isEmpty()) {
        setStatus(text("emptyPath"), false);
        return;
    }

    const QFileInfo info(rawPath);
    if (!info.exists() || !info.isDir()) {
        setStatus(text("missing"), false);
        return;
    }

    const QString path = normalizedPath(rawPath);
    if (m_library->folders().contains(path, Qt::CaseInsensitive)) {
        setStatus(text("duplicate"), false);
        return;
    }

    const int changed = m_library->addFolder(path);
    const bool added = m_library->folders().contains(path, Qt::CaseInsensitive);
    refreshFolders();

    if (added) {
        setStatus(text("added").arg(changed), true);
        emit foldersChanged();
    } else {
        setStatus(text("addFailed"), false);
    }
}

void FolderManagerDialog::removeSelectedFolder()
{
    if (!m_library) {
        return;
    }

    QListWidgetItem *item = m_folderList->currentItem();
    if (!item) {
        setStatus(text("selectRemove"), false);
        return;
    }

    const QString path = item->text();
    const auto result = QMessageBox::question(this,
                                              text("remove"),
                                              text("confirmRemove").arg(path));
    if (result != QMessageBox::Yes) {
        return;
    }

    if (m_library->removeFolder(path)) {
        refreshFolders();
        setStatus(text("removed"), true);
        emit foldersChanged();
    } else {
        setStatus(text("removeFailed"), false);
    }
}

void FolderManagerDialog::rescanFolders()
{
    if (!m_library) {
        return;
    }

    const int changed = m_library->rescanFolders();
    refreshFolders();
    setStatus(text("rescanned").arg(changed), true);
    emit foldersChanged();
}

void FolderManagerDialog::setStatus(const QString &message, bool ok)
{
    if (message.isEmpty()) {
        m_statusLabel->clear();
        return;
    }

    m_statusLabel->setProperty("state", ok ? "ok" : "error");
    m_statusLabel->style()->unpolish(m_statusLabel);
    m_statusLabel->style()->polish(m_statusLabel);
    m_statusLabel->setText(message);
}
