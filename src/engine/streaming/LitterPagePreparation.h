#ifndef OUTSHINE_ENGINE_STREAMING_LITTERPAGEPREPARATION_H
#define OUTSHINE_ENGINE_STREAMING_LITTERPAGEPREPARATION_H

#include "AssetPipeline.h"
#include "PreparedLitterPage.h"
#include <cstddef>
#include <expected>
#include <memory>

namespace outshine {
enum class LitterPageMiss { Generate, Refuse };

// Consumer scope identifies the cache store; select the lattice tile/mip before submission.
// IO/generation uses admitted workers; their pool outlives pending tickets.
[[nodiscard]] std::expected<AssetPipeline::RequestTicket, AssetPublicationError>
RequestLitterPage(AssetPipeline &pipeline,
                  const AssetPipeline::Consumer &consumer,
                  std::shared_ptr<AssetCache> cache,
                  Generators::LitterPageAddress address,
                  LitterPageMiss onMiss = LitterPageMiss::Generate,
                  size_t consumerBytesMost = 0);
}
#endif
