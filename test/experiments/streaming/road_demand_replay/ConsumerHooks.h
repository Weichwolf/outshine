#ifndef OUTSHINE_TEST_ROADDEMAND_HOOKS_H
#define OUTSHINE_TEST_ROADDEMAND_HOOKS_H

#include "RoadPageRestoration.h"
#include "PreparedRoadPages.h"
#include <Engine.h>
#include <AssetCache.h>
#include <cstddef>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>

using Restore = outshine::RoadPageRestoration;
using BeginResult = std::expected<std::unique_ptr<Restore>, std::string>;
using LoadResult = std::expected<std::optional<outshine::Generators::RoadPageLease>, std::string>;
using ResolveResult = std::expected<outshine::Generators::RoadPageLease, std::string>;
using CacheResult = std::expected<std::optional<outshine::CachedAsset>, outshine::AssetCacheError>;
BeginResult RealBegin(
    outshine::Tasks &pool,
    std::shared_ptr<outshine::Generators::PreparedRoadPages> pages,
    std::shared_ptr<const outshine::Generators::RoadPageManifest> manifest,
    std::optional<std::span<const uint32_t>> selected,
    std::string key,
    Restore::Limits
        limits) asm("__real__ZN8outshine19RoadPageRestoration5BeginERNS_5TasksENSt3__110shared_"
                    "ptrINS_10Generators17PreparedRoadPagesEEENS4_IKNS5_16RoadPageManifestEEENS3_"
                    "8optionalINS3_4spanIKjLm18446744073709551615EEEEENS3_12basic_stringIcNS3_"
                    "11char_traitsIcEENS3_9allocatorIcEEEENS0_6LimitsE");
BeginResult ProbeBegin(
    outshine::Tasks &pool,
    std::shared_ptr<outshine::Generators::PreparedRoadPages> pages,
    std::shared_ptr<const outshine::Generators::RoadPageManifest> manifest,
    std::optional<std::span<const uint32_t>> selected,
    std::string key,
    Restore::Limits
        limits) asm("__wrap__ZN8outshine19RoadPageRestoration5BeginERNS_5TasksENSt3__110shared_"
                    "ptrINS_10Generators17PreparedRoadPagesEEENS4_IKNS5_16RoadPageManifestEEENS3_"
                    "8optionalINS3_4spanIKjLm18446744073709551615EEEEENS3_12basic_stringIcNS3_"
                    "11char_traitsIcEENS3_9allocatorIcEEEENS0_6LimitsE");
LoadResult
ProbeLoad(outshine::Generators::PreparedRoadPages *pages, const std::string &key, size_t limit) asm(
    "__wrap__ZN8outshine10Generators17PreparedRoadPages7LoadKeyERKNSt3__112basic_stringIcNS2_"
    "11char_traitsIcEENS2_9allocatorIcEEEEm");
ResolveResult ProbeResolve(
    outshine::Generators::PreparedRoadPages *pages,
    const outshine::Generators::RoadPageRequest &request,
    const outshine::Generators::RoadPageProducer *producer,
    std::stop_token stop) asm("__wrap__ZN8outshine10Generators17PreparedRoadPages7ResolveERKNS0_"
                              "15RoadPageRequestEPKNS0_16RoadPageProducerENSt3__110stop_tokenE");
outshine::Result
RealRender(outshine::Engine *engine,
           outshine::Extent extent) asm("__real__ZN8outshine6Engine6renderENS_6ExtentE");
outshine::Result
ProbeRender(outshine::Engine *engine,
            outshine::Extent extent) asm("__wrap__ZN8outshine6Engine6renderENS_6ExtentE");
CacheResult
RealCacheLoad(const outshine::AssetCache *cache, std::string_view key, size_t limit) asm(
    "__real__ZNK8outshine10AssetCache4LoadENSt3__117basic_string_viewIcNS1_11char_traitsIcEEEEm");
CacheResult
ProbeCacheLoad(const outshine::AssetCache *cache, std::string_view key, size_t limit) asm(
    "__wrap__ZNK8outshine10AssetCache4LoadENSt3__117basic_string_viewIcNS1_11char_traitsIcEEEEm");
LoadResult
RealLoad(outshine::Generators::PreparedRoadPages *pages,
         const std::string &key,
         size_t limit) asm("__real__ZN8outshine10Generators17PreparedRoadPages7LoadKeyERKNSt3_"
                           "_112basic_stringIcNS2_11char_traitsIcEENS2_9allocatorIcEEEEm");

#endif
