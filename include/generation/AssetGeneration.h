#ifndef OUTSHINE_GENERATION_ASSETGENERATION_H
#define OUTSHINE_GENERATION_ASSETGENERATION_H

#include "content/AssetCache.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace outshine {

/// Complete owned native package produced on a cache miss; formats stay with the generator.
struct GeneratedAssetPackage {
  std::vector<AssetRecord> Records; ///< Slices, bounds, kinds and hierarchy metadata.
  std::vector<uint8_t> Bytes;       ///< Immutable package payload, within the supplied byte budget.
};

/// Synchronous miss factory receiving the whole-package byte budget. Invoked at most once by
/// ResolveAsset; optional providers are implementation details. Run expensive factories on a
/// worker.
using AssetFactory = std::function<std::expected<GeneratedAssetPackage, std::string>(size_t)>;

/// Load the native product first. Only a miss invokes factory, atomically publishes its complete
/// package and loads through the same path. Factory is borrowed for this call; an empty factory
/// is valid on hits and fails on misses. The requested key must occur in the generated records.
/// Factory failures and invalid packages publish nothing; a successful publication may precede
/// a load error. Checksum/slice corruption is a miss; native format validation stays with the
/// caller. Serialize cache access for the entire call.
/// @param cache Shared native store used by builtins and external generators.
/// @param key Stable SHA-256 product identity binding all persistent inputs and parameters.
/// @param packageBytesMost Upper bound on the whole package's encoded bytes.
/// @param factory Optional cold-path generator; returned failures remain unchanged.
/// @return Owned immutable product lease or a diagnostic error; may perform disk IO and allocate.
[[nodiscard]] std::expected<CachedAsset, std::string> ResolveAsset(AssetCache &cache,
                                                                   std::string_view key,
                                                                   size_t packageBytesMost,
                                                                   const AssetFactory &factory);

}
#endif
