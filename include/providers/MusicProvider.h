#pragma once

#include <QObject>
#include <QString>

enum class ProviderAccess
{
    NotConfigured,
    ExternalLaunch,
    Playable
};

struct ProviderInfo
{
    QString id;
    QString displayName;
    ProviderAccess access = ProviderAccess::NotConfigured;
    QString status;
    QString detail;
};

class MusicProvider : public QObject
{
    Q_OBJECT

public:
    explicit MusicProvider(QObject *parent = nullptr) : QObject(parent) {}
    ~MusicProvider() override = default;

    virtual QString id() const = 0;
    virtual QString displayName() const = 0;
    virtual ProviderInfo info() const = 0;
};
