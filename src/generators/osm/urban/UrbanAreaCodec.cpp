#include "UrbanAreaCodec.h"
#include "BinaryValueArchive.h"
#include <world/data/DataKind.h>
#include "TileSourceIdentity.h"
#include <world/data/Address.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <vector>
#include <utility>

namespace outshine::Generators::Osm {
namespace {
constexpr uint64_t kFormat = 0x31414552414e4255;
constexpr size_t kCellsMost = 4194304;
constexpr uint32_t kInputsMost = 16;
constexpr uint32_t kSourcesMost = 8192;
constexpr size_t kTextBytesMost = 4096;

bool ValidInput(const UrbanAreaInput &input) {
  const auto &id = input.Identity;
  const auto zoom = static_cast<uint32_t>(id.Tile.Zoom);
  if (zoom > static_cast<uint32_t>(Data::TileId::MaximumZoom)) { return false; }
  const uint32_t side = uint32_t{1} << zoom;
  return static_cast<uint32_t>(id.From) <=
             static_cast<uint32_t>(Data::TileSourceIdentity::Origin::Shaped) &&
         id.Kind == Data::DataKind::VectorMap && !id.NativeCell && id.Tile.X < side &&
         id.Tile.Y < side && id.SourceId.size() <= kTextBytesMost &&
         id.Revision.size() <= kTextBytesMost && input.PayloadDigest.size() <= kTextBytesMost;
}

bool Valid(const UrbanArea &area) {
  const auto &recipe = area.Recipe;
  const uint64_t cells = static_cast<uint64_t>(area.Columns) * area.Rows;
  if (!std::isfinite(recipe.ProjectedPitchM) || recipe.ProjectedPitchM < 8 ||
      recipe.ProjectedPitchM > 512 || !std::isfinite(recipe.FillFraction) ||
      recipe.FillFraction <= 0 || recipe.FillFraction >= 1 || recipe.PatchSide == 0 ||
      recipe.PatchSide > 32 || !std::isfinite(area.MetresPerProjectedMetre) ||
      area.MetresPerProjectedMetre <= 0 || area.MetresPerProjectedMetre > 1 ||
      !std::ranges::all_of(area.ProjectedOriginM, [](double v) { return std::isfinite(v); }) ||
      ((area.Columns == 0) != (area.Rows == 0)) || cells > kCellsMost ||
      area.Tiers.size() != cells || area.Inputs.empty() || area.Inputs.size() > kInputsMost ||
      area.Sources.size() > kSourcesMost) {
    return false;
  }
  const auto occupied = static_cast<size_t>(
      std::ranges::count_if(area.Tiers, [](uint8_t tier) { return tier != 0; }));
  const bool urban = std::ranges::any_of(
      area.Sources, [](const auto &source) { return source.Role == UrbanSourceRole::Urban; });
  return occupied == area.Occupied && area.HasUrbanEvidence == urban && (urban || occupied == 0) &&
         std::ranges::all_of(area.Tiers,
                             [](uint8_t tier) { return tier < UrbanArea::HeightsM.size(); }) &&
         std::ranges::all_of(area.Inputs, ValidInput) &&
         std::ranges::all_of(area.Sources, [&](const auto &source) {
           return source.Input < area.Inputs.size() && source.Role <= UrbanSourceRole::Tunnel;
         });
}

bool WriteInput(BinaryValueWriter &writer, const UrbanAreaInput &input) {
  const auto &id = input.Identity;
  return writer(id.From, id.Kind, id.Tile.Zoom, id.Tile.X, id.Tile.Y) && writer.Text(id.SourceId) &&
         writer.Text(id.Revision) && writer.Text(input.PayloadDigest);
}

bool ReadInput(BinaryValueReader &reader, UrbanAreaInput &input) {
  auto &id = input.Identity;
  return reader(id.From, id.Kind, id.Tile.Zoom, id.Tile.X, id.Tile.Y) && reader.Text(id.SourceId) &&
         reader.Text(id.Revision) && reader.Text(input.PayloadDigest) && ValidInput(input);
}

bool WriteSource(BinaryValueWriter &writer, const UrbanSourceUse &source) {
  return writer(source.Input, source.Feature, source.Role, source.ProviderFeatureId.has_value()) &&
         (!source.ProviderFeatureId || writer(*source.ProviderFeatureId));
}

bool ReadSource(BinaryValueReader &reader, UrbanSourceUse &source) {
  bool identified = false;
  if (!reader(source.Input, source.Feature, source.Role, identified)) { return false; }
  if (identified) {
    uint64_t id = 0;
    if (!reader(id)) { return false; }
    source.ProviderFeatureId = id;
  }
  return true;
}
}

std::expected<std::vector<uint8_t>, std::string> EncodeUrbanArea(const UrbanArea &area,
                                                                 size_t bytesMost) {
  if (!Valid(area)) { return std::unexpected("invalid native urban-area product"); }
  BinaryValueWriter writer(std::min(bytesMost, kUrbanAreaBytesMost));
  if (!writer(kFormat,
              area.Recipe.ProjectedPitchM,
              area.Recipe.FillFraction,
              area.Recipe.PatchSide,
              area.FirstCell,
              area.ProjectedOriginM,
              area.MetresPerProjectedMetre,
              area.Columns,
              area.Rows,
              static_cast<uint32_t>(area.Occupied),
              area.HasUrbanEvidence,
              static_cast<uint32_t>(area.Inputs.size())) ||
      !std::ranges::all_of(area.Inputs,
                           [&](const auto &input) { return WriteInput(writer, input); }) ||
      !writer(static_cast<uint32_t>(area.Sources.size())) ||
      !std::ranges::all_of(area.Sources,
                           [&](const auto &source) { return WriteSource(writer, source); }) ||
      !writer.Array(std::span(area.Tiers))) {
    return std::unexpected("native urban-area encoding exceeds byte budget");
  }
  return std::move(writer.Out).TakeBytes();
}

std::expected<UrbanArea, std::string> DecodeUrbanArea(std::span<const uint8_t> bytes,
                                                      size_t bytesMost) {
  if (bytes.size() > std::min(bytesMost, kUrbanAreaBytesMost)) {
    return std::unexpected("native urban-area input exceeds byte budget");
  }
  BinaryValueReader reader(bytes);
  UrbanArea area;
  uint64_t format = 0;
  uint32_t occupied = 0;
  uint32_t count = 0;
  if (!reader(format,
              area.Recipe.ProjectedPitchM,
              area.Recipe.FillFraction,
              area.Recipe.PatchSide,
              area.FirstCell,
              area.ProjectedOriginM,
              area.MetresPerProjectedMetre,
              area.Columns,
              area.Rows,
              occupied,
              area.HasUrbanEvidence,
              count) ||
      format != kFormat || count == 0 || count > kInputsMost ||
      static_cast<uint64_t>(area.Columns) * area.Rows > kCellsMost) {
    return std::unexpected("invalid native urban-area header");
  }
  area.Occupied = occupied;
  area.Inputs.resize(count);
  if (!std::ranges::all_of(area.Inputs, [&](auto &input) { return ReadInput(reader, input); }) ||
      !reader(count) || count > kSourcesMost) {
    return std::unexpected("invalid native urban-area source receipts");
  }
  area.Sources.resize(count);
  if (!std::ranges::all_of(area.Sources,
                           [&](auto &source) { return ReadSource(reader, source); }) ||
      !reader.Array(area.Tiers) || reader.In.Remaining() != 0 || !Valid(area)) {
    return std::unexpected("invalid native urban-area field or provenance");
  }
  return area;
}
}
