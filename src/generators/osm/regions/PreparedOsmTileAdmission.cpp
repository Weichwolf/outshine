#include "PreparedOsmTileWorker.h"
#include <algorithm>
#include <cstddef>
#include <expected>
#include <initializer_list>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace outshine::Generators::Osm {
namespace {
std::optional<size_t> Sum(std::initializer_list<size_t> parts) {
  size_t bytes = 0;
  for (const auto part : parts) {
    if (part > std::numeric_limits<size_t>::max() - bytes) { return std::nullopt; }
    bytes += part;
  }
  return bytes;
}
}

std::expected<AssetPipeline::LogicalStep, std::string>
PreparedOsmTiles::Worker::PlanGeneration(Production &work) {
  if (PublicationBytes_.load() >= Budget_.PublicationBytesMost) {
    return std::optional<AssetProduct>{};
  }
  if (!work.Landing) { return std::unexpected("native OSM generation has no input"); }
  if (work.GenerationMost == 0) {
    const auto &input = *work.Landing;
    const auto build =
        OsmField::NativeTileBuildBytes(input.Bytes, work.Definition.Layers, work.Definition.Schema);
    if (!build) { return std::unexpected(std::string(build.error())); }
    const auto encoded =
        Sum({build->EncodedBytesMost, input.SourceId.size(), input.SourceRevision.size()});
    if (!encoded) { return std::unexpected("native OSM package size plan overflows"); }
    const auto outputMost = std::min(kPackageBytesMost, *encoded);
    const auto codec = AssetCache::PublicationBufferBytes(outputMost);
    if (!codec) { return std::unexpected("native OSM codec storage plan failed"); }
    const auto generation = Sum({build->WorkingBytesMost,
                                 input.Bytes.capacity(),
                                 input.SourceId.capacity(),
                                 input.SourceRevision.capacity(),
                                 outputMost,
                                 kReceiptBytesMost});
    const auto publication = Sum({outputMost, *codec, kReceiptBytesMost});
    if (!generation || !publication) {
      return std::unexpected("native OSM generation storage plan overflows");
    }
    const auto bytes = std::max(*generation, *publication);
    work.GenerationMost = bytes;
  }
  work.Stage = Production::Phase::Generate;
  return AssetPipeline::InputAdmission{.Bytes = work.GenerationMost};
}

std::expected<AssetPipeline::LogicalStep, std::string>
PreparedOsmTiles::Worker::PrepareDelivery(Production &work, CachedAsset native) {
  const auto decoded = OsmField::NativeTileDecodeBytes(native.Bytes(), kHeapBytesMost);
  const auto consumer = Sum({decoded.value_or(work.LoadPlan.BufferBytesMost), kReceiptBytesMost});
  if (!consumer) { return std::unexpected("native OSM consumer storage plan overflows"); }
  AssetProduct product(std::move(native), *consumer);
  if (work.Stage == Production::Phase::Hit) { product.SetRequestBinding(work.Request); }
  const auto total = Sum({product.BackingBytes(), *consumer});
  if (!total) { return std::unexpected("native OSM delivery storage plan overflows"); }
  SetPublication(work, std::nullopt);
  work.Landing.reset();
  work.Delivery.emplace(std::move(product));
  work.Stage = Production::Phase::Deliver;
  return AssetPipeline::InputAdmission{.Bytes = *total};
}

std::expected<AssetPipeline::LogicalStep, std::string>
PreparedOsmTiles::Worker::Deliver(Production &work) {
  if (!work.Delivery) { return std::unexpected("native OSM delivery has no prepared product"); }
  auto product = std::move(*work.Delivery);
  work.Delivery.reset();
  return std::optional<AssetProduct>(std::move(product));
}

}
