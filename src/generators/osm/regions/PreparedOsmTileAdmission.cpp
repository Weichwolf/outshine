#include "PreparedOsmTiles.h"
#include <algorithm>
#include <cstddef>
#include <expected>
#include <initializer_list>
#include <limits>
#include <optional>
#include <string>

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
PreparedOsmTiles::PlanGeneration(Production &work) {
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

}
