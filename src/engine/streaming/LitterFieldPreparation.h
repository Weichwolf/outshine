#ifndef OUTSHINE_ENGINE_STREAMING_LITTERFIELDPREPARATION_H
#define OUTSHINE_ENGINE_STREAMING_LITTERFIELDPREPARATION_H

#include "AssetPipeline.h"
#include "PreparedLitterField.h"
#include <cstddef>
#include <expected>
#include <memory>

namespace outshine {
enum class LitterFieldMiss { Generate, Refuse };

// Consumer scope identifies the cache store; select the lattice tile/mip before submission.
// IO/generation uses admitted workers; their pool outlives pending tickets.
[[nodiscard]] std::expected<AssetPipeline::RequestTicket, AssetPublicationError>
RequestLitterField(AssetPipeline &pipeline,
                   const AssetPipeline::Consumer &consumer,
                   std::shared_ptr<AssetCache> cache,
                   Generators::LitterFieldAddress address,
                   LitterFieldMiss onMiss = LitterFieldMiss::Generate,
                   size_t consumerBytesMost = 0);
}
#endif
