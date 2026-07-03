#include "providers/ProviderRegistry.h"

QList<ProviderInfo> ProviderRegistry::providers() const
{
    return {
        {
            "qqmusic",
            "QQ Music",
            ProviderAccess::NotConfigured,
            "Not configured",
            "Requires official authorization."
        },
        {
            "netease",
            "NetEase Cloud Music",
            ProviderAccess::NotConfigured,
            "Not configured",
            "Requires official authorization."
        },
        {
            "kugou",
            "Kugou Music",
            ProviderAccess::NotConfigured,
            "Not configured",
            "Requires official authorization."
        }
    };
}
