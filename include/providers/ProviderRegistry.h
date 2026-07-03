#pragma once

#include "providers/MusicProvider.h"

#include <QList>

class ProviderRegistry
{
public:
    QList<ProviderInfo> providers() const;
};
