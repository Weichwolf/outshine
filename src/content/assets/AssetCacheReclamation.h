#ifndef OUTSHINE_CONTENT_ASSETS_ASSETCACHERECLAMATION_H
#define OUTSHINE_CONTENT_ASSETS_ASSETCACHERECLAMATION_H

#include "AssetCache.h"
#include "SqliteFileBudget.h"
#include <expected>

namespace outshine::AssetSql {
// Storage worker only; one package or at most 256 free pages per step.
[[nodiscard]] std::expected<bool, AssetCacheError> ReclaimStep(const SqliteFileBudget &budget);
}
#endif
