#include "GroundPortTerrain.h"
#include "HeightField.h"
#include "TerrainFieldSet.h"
#include "TerrainGrid.h"
#include "TileSourceIdentity.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <stop_token>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {
using Error = GroundPortTerrainError;

bool Original(const Ground::HeightField::Block &block, int zoom) {
  if (!block.Terrain || !block.Nodes.empty() || block.At.Zoom != zoom ||
      std::cmp_less(block.At.X, 0) || std::cmp_less(block.At.Y, 0) || block.MissingBoundary ||
      block.Terrain->HasMissingBoundary() || !Ground::HeightField::HasRaster(block)) {
    return false;
  }
  const auto cells = uint64_t{1} << zoom;
  return std::cmp_less(block.At.X, cells) && std::cmp_less(block.At.Y, cells) &&
         block.Raster.Postings == block.Terrain->Cols() &&
         std::ranges::equal(block.Sources, block.Terrain->Sources());
}

bool MetadataFits(const Ground::HeightField &field,
                  GroundPortTerrainLimits limits,
                  std::string_view datum,
                  std::string_view mapping) {
  constexpr size_t kBlockBytes = 128;
  constexpr size_t kSourceBytes = 64;
  size_t left = limits.MetadataBytes;
  const auto charge = [&left](size_t bytes) {
    if (bytes > left) { return false; }
    left -= bytes;
    return true;
  };
  if (!charge(datum.size()) || !charge(mapping.size()) || !charge(kBlockBytes)) { return false; }
  for (const auto &block : field.Blocks()) {
    if (!charge(kBlockBytes)) { return false; }
    for (const auto &source : block.Sources) {
      if (!charge(kSourceBytes) || !charge(source.SourceId.size()) ||
          !charge(source.Revision.size())) {
        return false;
      }
    }
  }
  return true;
}
}

GroundPortTerrain::GroundPortTerrain(std::shared_ptr<const Ground::HeightField> field,
                                     TransportInputReference input,
                                     GroundPortTerrainWork work)
    : Field_(std::move(field)), Input_(std::move(input)), Work_(work) {}

std::expected<GroundPortTerrain, GroundPortTerrainError>
GroundPortTerrain::Bind(std::shared_ptr<const Ground::HeightField> field,
                        std::string_view sourceDatum,
                        std::string_view heightMapping,
                        GroundPortTerrainLimits limits,
                        const std::stop_token &stop) {
  if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
  if (!field || !field->Qualified()) { return std::unexpected(Error::MissingTerrain); }
  if (sourceDatum.empty() || heightMapping != ModelHeightMapping || field->Zoom() < 0 ||
      field->Zoom() > Ground::HeightField::MaximumTileZoom) {
    return std::unexpected(Error::InvalidInput);
  }
  if (field->Blocks().size() > limits.Tiles ||
      !MetadataFits(*field, limits, sourceDatum, heightMapping)) {
    return std::unexpected(Error::CapacityExceeded);
  }
  size_t bytes = 0;
  for (const auto &block : field->Blocks()) {
    if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
    if (!Original(block, field->Zoom())) { return std::unexpected(Error::InvalidInput); }
    const auto side = static_cast<size_t>(block.Raster.Side);
    if (side > (limits.RasterBytes - bytes) / sizeof(float) / side) {
      return std::unexpected(Error::CapacityExceeded);
    }
    bytes += side * side * sizeof(float);
  }
  std::vector<TerrainFieldSet::Entry> entries;
  entries.reserve(field->Blocks().size());
  for (const auto &block : field->Blocks()) {
    entries.emplace_back(Data::TileId{.Zoom = block.At.Zoom,
                                      .X = static_cast<uint32_t>(block.At.X),
                                      .Y = static_cast<uint32_t>(block.At.Y)},
                         block.Terrain);
  }
  const TerrainFieldSet fields(std::move(entries));
  const auto binding = fields.BilinearBindingAt(field->Zoom(), sourceDatum, heightMapping);
  if (!binding) {
    return std::unexpected(binding.error() == TerrainFieldSet::BindingError::OverBudget
                               ? Error::CapacityExceeded
                               : Error::InvalidInput);
  }
  if (binding->MetadataBytes > limits.MetadataBytes) {
    return std::unexpected(Error::CapacityExceeded);
  }
  if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
  return GroundPortTerrain(std::move(field),
                           {.DatasetId = "outshine/terrain-bilinear-view",
                            .Version = "1",
                            .Revision = "",
                            .ItemId = "original-raster",
                            .Content = binding->Key},
                           {.Fields = binding->Fields,
                            .RasterBytes = binding->RasterBytes,
                            .MetadataBytes = binding->MetadataBytes,
                            .CopiedRasterBytes = binding->CopiedRasterBytes});
}

std::optional<double> GroundPortTerrain::At(LongitudeLatitude at) const noexcept {
  const auto sampled = Field_->SampleBilinearAt(at);
  return sampled ? sampled->Height.AslM() : std::nullopt;
}
}
