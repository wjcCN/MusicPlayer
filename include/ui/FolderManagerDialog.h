#pragma once

#include <QDialog>

class QLabel;
class QLineEdit;
class QListWidget;
class MusicLibrary;

class FolderManagerDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit FolderManagerDialog(MusicLibrary *library, bool english, QWidget *parent = nullptr);

signals:
    void foldersChanged();

private:
    QString text(const char *key) const;
    QString normalizedPath(const QString &path) const;
    void refreshFolders();
    void browseFolder();
    void addFolder();
    void removeSelectedFolder();
    void rescanFolders();
    void setStatus(const QString &message, bool ok = true);

    MusicLibrary *m_library = nullptr;
    bool m_english = false;
    QListWidget *m_folderList = nullptr;
    QLineEdit *m_pathEdit = nullptr;
    QLabel *m_statusLabel = nullptr;
};
