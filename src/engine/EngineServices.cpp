#include "EngineState.h"
#include "AssetPipeline.h"
#include <cstddef>
#include <memory>

namespace outshine {
std::shared_ptr<AssetPipeline> EngineServices::AssetPublications() {
  if (!Compute) { return {}; }
  if (!Publications) {
    constexpr size_t mebibyte = size_t{1024} * 1024;
    Publications = std::make_shared<AssetPipeline>(
        *Compute,
        AssetPipeline::Limits{.JobsMost = 4,
                              .ConsumersMost = 32,
                              .TicketsMost = 32,
                              .InputBytesMost = 128 * mebibyte,
                              .ReceiptBytesMost = size_t{32} * 4096});
  }
  return Publications;
}
}
