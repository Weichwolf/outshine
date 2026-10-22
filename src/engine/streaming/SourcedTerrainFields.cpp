#include "SourcedTerrainFields.h"

#include <algorithm>
#include <utility>
#include <cstdint>
#include <cstddef>
#include <expected>
#include <cmath>
#include <optional>
#include <array>
#include <limits>
#include <ranges>
#include <span>

namespace outshine {
namespace {
enum class SourceUse { Capture, Sampling };
constexpr double kInfinity = std::numeric_limits<double>::infinity();
constexpr float kFloatInfinity = std::numeric_limits<float>::infinity();

std::span<const SourcedTerrainFields::Entry>::iterator
SourceFor(std::span<const SourcedTerrainFields::Entry> fields,
          Data::TileId tile,
          SourceUse use = SourceUse::Capture) {
  for (int zoom = tile.Zoom; zoom >= 0; --zoom) {
    const auto drop = static_cast<uint32_t>(tile.Zoom - zoom);
    const Data::TileId source{.Zoom = zoom, .X = tile.X >> drop, .Y = tile.Y >> drop};
    const auto found =
        std::ranges::find_if(fields, [source](const auto &entry) { return entry.first == source; });
    if (found != fields.end() && found->second && found->second->Meshable() &&
        (use == SourceUse::Sampling || !found->second->Sources().empty())) {
      return found;
    }
  }
  return fields.end();
}

using RangeError = SourcedTerrainFields::RangeError;

struct SampleRectangle {
  Ground::TileFrac First, Last;
};

std::expected<SampleRectangle, RangeError> RangeRectangle(int zoom, Ground::GeoBounds bounds) {
  if (zoom < 0 || zoom > Ground::HeightField::MaximumTileZoom || !std::isfinite(bounds.MinLonDeg) ||
      !std::isfinite(bounds.MaxLonDeg) || !std::isfinite(bounds.MinLatDeg) ||
      !std::isfinite(bounds.MaxLatDeg) || bounds.MinLonDeg > bounds.MaxLonDeg ||
      bounds.MinLatDeg > bounds.MaxLatDeg || bounds.MinLonDeg < -kDegPerHalfTurn ||
      bounds.MaxLonDeg >= kDegPerHalfTurn || bounds.MinLatDeg <= -kMercatorLatMaxDeg ||
      bounds.MaxLatDeg >= kMercatorLatMaxDeg) {
    return std::unexpected(RangeError::InvalidRequest);
  }
  const auto first = Ground::ToTileFracClamped(
      {.LongitudeDeg = bounds.MinLonDeg, .LatitudeDeg = bounds.MaxLatDeg}, zoom);
  const auto last = Ground::ToTileFracClamped(
      {.LongitudeDeg = bounds.MaxLonDeg, .LatitudeDeg = bounds.MinLatDeg}, zoom);
  const double span = std::ldexp(1.0, zoom);
  if (first.X < 0 || first.Y < 0 || last.X >= span || last.Y >= span) {
    return std::unexpected(RangeError::InvalidRequest);
  }
  const double inside = std::nextafter(span, 0.0);
  return SampleRectangle{.First = {.X = std::max(0.0, std::nextafter(first.X, -kInfinity)),
                                   .Y = std::max(0.0, std::nextafter(first.Y, -kInfinity))},
                         .Last = {.X = std::min(inside, std::nextafter(last.X, kInfinity)),
                                  .Y = std::min(inside, std::nextafter(last.Y, kInfinity))}};
}

std::array<uint32_t, 2> SupportIndices(double first, double last, uint32_t count) {
  const double end = count - 1u;
  return {static_cast<uint32_t>(std::max(0.0, std::floor(first * end) - 1)),
          static_cast<uint32_t>(std::min(end, std::ceil(last * end) + 1))};
}

std::expected<void, RangeError> IncludeRange(const SourcedTerrainFields::Entry &source,
                                             int zoom,
                                             SampleRectangle rectangle,
                                             size_t nodesMost,
                                             SourcedTerrainFields::HeightRange &range) {
  const auto &field = *source.second;
  if (field.HasMissingBoundary()) { return std::unexpected(RangeError::IncompleteSource); }
  const double scale = std::ldexp(1.0, source.first.Zoom - zoom);
  const auto columns =
      SupportIndices(std::clamp(rectangle.First.X * scale - source.first.X, 0.0, 1.0),
                     std::clamp(rectangle.Last.X * scale - source.first.X, 0.0, 1.0),
                     field.Cols());
  const auto rows = SupportIndices(std::clamp(rectangle.First.Y * scale - source.first.Y, 0.0, 1.0),
                                   std::clamp(rectangle.Last.Y * scale - source.first.Y, 0.0, 1.0),
                                   field.Rows());
  const size_t count = (static_cast<size_t>(columns[1]) - columns[0] + 1) *
                       (static_cast<size_t>(rows[1]) - rows[0] + 1);
  if (count > nodesMost - range.Nodes) { return std::unexpected(RangeError::OverBudget); }
  for (uint32_t row = rows[0]; row <= rows[1]; ++row) {
    for (uint32_t column = columns[0]; column <= columns[1]; ++column) {
      const float value = field.AtM(row, column);
      if (!std::isfinite(value)) { return std::unexpected(RangeError::IncompleteSource); }
      range.AslM[0] = std::min(range.AslM[0], static_cast<double>(value));
      range.AslM[1] = std::max(range.AslM[1], static_cast<double>(value));
      ++range.Nodes;
    }
  }
  return {};
}

std::optional<Data::TileId> RequestedTile(Ground::TileSpot request) {
  if (request.Zoom < 0 || request.Zoom > Ground::HeightField::MaximumTileZoom || request.X < 0 ||
      request.Y < 0 ||
      static_cast<uint64_t>(request.X) >= (uint64_t{1} << static_cast<uint32_t>(request.Zoom)) ||
      static_cast<uint64_t>(request.Y) >= (uint64_t{1} << static_cast<uint32_t>(request.Zoom))) {
    return std::nullopt;
  }
  return Data::TileId{.Zoom = request.Zoom,
                      .X = static_cast<uint32_t>(request.X),
                      .Y = static_cast<uint32_t>(request.Y)};
}

}

size_t SourcedTerrainFields::RetainedBytes() const noexcept {
  size_t bytes = PreparationBytes();
  if (Metadata_) { bytes += Metadata_->HeapBytes(); }
  for (auto at = Fields_.begin(); at != Fields_.end(); ++at) {
    if (at->second && std::none_of(Fields_.begin(), at, [&at](const Entry &entry) {
          return entry.second == at->second;
        })) {
      bytes += at->second->HeapBytes();
    }
  }
  return bytes;
}

size_t SourcedTerrainFields::PreparationBytes() const noexcept {
  return Fields_.capacity() * sizeof(Entry);
}

std::expected<SourcedTerrainFields, SourcedTerrainFields::CaptureError>
SourcedTerrainFields::Capture(std::span<const Entry> fields,
                              std::span<const Ground::TileSpot> requests,
                              size_t bytesMost,
                              Ground::TerrainRevisionIndex::Reservation metadata) {
  if (requests.size() > bytesMost / sizeof(Entry)) {
    return std::unexpected(CaptureError::OverBudget);
  }
  SourcedTerrainFields captured;
  captured.Metadata_ = std::move(metadata);
  captured.Fields_.reserve(requests.size());
  if (captured.PreparationBytes() > bytesMost) { return std::unexpected(CaptureError::OverBudget); }
  for (const Ground::TileSpot request : requests) {
    const auto tile = RequestedTile(request);
    if (!tile) { return std::unexpected(CaptureError::InvalidRequest); }
    const auto found = SourceFor(fields, *tile);
    if (found == fields.end()) { return std::unexpected(CaptureError::MissingSource); }
    if (std::ranges::any_of(captured.Fields_,
                            [&found](const Entry &entry) { return entry.first == found->first; })) {
      continue;
    }
    captured.Fields_.push_back(*found);
  }
  return captured;
}

bool SourcedTerrainFields::FitsPreparation(std::span<const Ground::TileSpot> requests,
                                           size_t bytesMost) const {
  size_t bytes = PreparationBytes();
  const auto charge = [&bytes, bytesMost](size_t count, size_t size) {
    if (bytes > bytesMost || (size != 0 && count > (bytesMost - bytes) / size)) { return false; }
    bytes += count * size;
    return true;
  };
  if (!charge(1, sizeof(Ground::HeightField)) ||
      !charge(requests.size(), sizeof(Ground::TileSpot) + sizeof(Ground::HeightField::Block))) {
    return false;
  }
  for (const auto request : requests) {
    const auto tile = RequestedTile(request);
    if (!tile) { return false; }
    const auto found = SourceFor(Fields_, *tile);
    if (found == Fields_.end()) { return false; }
    const auto &field = *found->second;
    if (field.Rows() != field.Cols()) { return false; }
    if (!charge(4, field.Certificate().HeapBytes())) { return false; }
    for (const auto &source : field.Sources()) {
      if (!charge(4,
                  sizeof(Data::TileSourceIdentity) + source.SourceId.capacity() +
                      source.Revision.capacity() + 2)) {
        return false;
      }
    }
    if (found->first != *tile) {
      const auto scale = uint64_t{1} << static_cast<uint32_t>(tile->Zoom - found->first.Zoom);
      const size_t side = std::min<size_t>((field.Cols() - 1u + scale - 1u) / scale + 1u, 257u);
      if (!charge(side * side, sizeof(float))) { return false; }
    }
  }
  return true;
}

bool SourcedTerrainFields::ShareSourcedField(Data::TileId tile,
                                             Ground::HeightField::Block &into) const {
  return Share(Fields_, tile, into);
}

bool SourcedTerrainFields::Share(std::span<const Entry> fields,
                                 Data::TileId tile,
                                 Ground::HeightField::Block &into) {
  if (tile.Zoom < 0 || tile.Zoom > Ground::HeightField::MaximumTileZoom) { return false; }
  const auto found = SourceFor(fields, tile);
  if (found == fields.end()) { return false; }
  return found->first == tile ? Ground::HeightField::SharesField(found->second, tile, into)
                              : Ground::HeightField::ResamplesSourcedAncestor(
                                    *found->second, found->first, tile, into);
}

bool SourcedTerrainFields::Copy(std::span<const Entry> fields,
                                Data::TileId tile,
                                Ground::HeightField::Block &into) {
  if (tile.Zoom < 0 || tile.Zoom > Ground::HeightField::MaximumTileZoom) { return false; }
  const auto found = SourceFor(fields, tile);
  if (found == fields.end()) { return false; }
  if (found->first == tile) { return Ground::HeightField::CopiesField(*found->second, tile, into); }
  return Ground::HeightField::ResamplesSourcedAncestor(*found->second, found->first, tile, into);
}

bool SourcedTerrainFields::CopySourcedField(Data::TileId tile,
                                            Ground::HeightField::Block &into) const {
  return Copy(Fields_, tile, into);
}

std::optional<double> SourcedTerrainFields::AslMAt(int zoom, LongitudeLatitude at) const {
  return AslMAt(Fields_, zoom, at);
}

std::optional<double>
SourcedTerrainFields::AslMAt(std::span<const Entry> fields, int zoom, LongitudeLatitude at) {
  for (int heldZoom = zoom; heldZoom >= 0; --heldZoom) {
    const Ground::TileFrac frac = Ground::ToTileFracClamped(
        {.LongitudeDeg = at.LongitudeDeg, .LatitudeDeg = at.LatitudeDeg}, heldZoom);
    const Data::TileId tile{.Zoom = heldZoom,
                            .X = static_cast<uint32_t>(std::floor(frac.X)),
                            .Y = static_cast<uint32_t>(std::floor(frac.Y))};
    const auto found =
        std::ranges::find_if(fields, [tile](const Entry &entry) { return entry.first == tile; });
    const Ground::TerrainField *field = found == fields.end() ? nullptr : found->second.get();
    if (field == nullptr || !field->Meshable()) { continue; }
    return field->PostingM(
        {.Col = frac.X - std::floor(frac.X), .Row = frac.Y - std::floor(frac.Y)});
  }
  return std::nullopt;
}

std::expected<SourcedTerrainFields::HeightRange, SourcedTerrainFields::RangeError>
SourcedTerrainFields::AslRangeOver(int zoom, Ground::GeoBounds bounds, size_t nodesMost) const {
  const auto rectangle = RangeRectangle(zoom, bounds);
  if (!rectangle) { return std::unexpected(rectangle.error()); }
  const auto firstX = static_cast<uint32_t>(rectangle->First.X);
  const auto firstY = static_cast<uint32_t>(rectangle->First.Y);
  const auto lastX = static_cast<uint32_t>(rectangle->Last.X);
  const auto lastY = static_cast<uint32_t>(rectangle->Last.Y);
  const uint64_t tiles = (uint64_t{lastX} - firstX + 1) * (uint64_t{lastY} - firstY + 1);
  if (tiles > nodesMost) { return std::unexpected(RangeError::OverBudget); }
  const std::span<const Entry> fields = Fields_;
  HeightRange range{.AslM = {kInfinity, -kInfinity}};
  for (uint32_t y = firstY; y <= lastY; ++y) {
    for (uint32_t x = firstX; x <= lastX; ++x) {
      const auto source = SourceFor(fields, {.Zoom = zoom, .X = x, .Y = y}, SourceUse::Sampling);
      if (source == fields.end()) { return std::unexpected(RangeError::MissingSource); }
      const SampleRectangle part{
          .First = {.X = std::max(rectangle->First.X, static_cast<double>(x)),
                    .Y = std::max(rectangle->First.Y, static_cast<double>(y))},
          .Last = {.X = std::min(rectangle->Last.X, x + 1.0),
                   .Y = std::min(rectangle->Last.Y, y + 1.0)}};
      const auto included = IncludeRange(*source, zoom, part, nodesMost, range);
      if (!included) { return std::unexpected(included.error()); }
    }
  }
  range.AslM[0] = std::nextafter(static_cast<float>(range.AslM[0]), -kFloatInfinity);
  range.AslM[1] = std::nextafter(static_cast<float>(range.AslM[1]), kFloatInfinity);
  if (!std::isfinite(range.AslM[0]) || !std::isfinite(range.AslM[1])) {
    return std::unexpected(RangeError::IncompleteSource);
  }
  return range;
}

}
