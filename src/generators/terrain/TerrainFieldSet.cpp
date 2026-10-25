#include "TerrainFieldSet.h"

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

namespace outshine::Generators {
namespace {
enum class SourceUse { Qualified, Sampling };
constexpr double kInfinity = std::numeric_limits<double>::infinity();
constexpr float kFloatInfinity = std::numeric_limits<float>::infinity();

std::span<const TerrainFieldSet::Entry>::iterator
SourceFor(std::span<const TerrainFieldSet::Entry> fields,
          Data::TileId tile,
          SourceUse use = SourceUse::Qualified) {
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

using RangeError = TerrainFieldSet::RangeError;

struct SampleRectangle {
  ::outshine::Ground::TileFrac First, Last;
};

std::expected<SampleRectangle, RangeError> RangeRectangle(int zoom,
                                                          ::outshine::Ground::GeoBounds bounds) {
  if (zoom < 0 || zoom > ::outshine::Ground::HeightField::MaximumTileZoom ||
      !std::isfinite(bounds.MinLonDeg) || !std::isfinite(bounds.MaxLonDeg) ||
      !std::isfinite(bounds.MinLatDeg) || !std::isfinite(bounds.MaxLatDeg) ||
      bounds.MinLonDeg > bounds.MaxLonDeg || bounds.MinLatDeg > bounds.MaxLatDeg ||
      bounds.MinLonDeg < -kDegPerHalfTurn || bounds.MaxLonDeg >= kDegPerHalfTurn ||
      bounds.MinLatDeg <= -kMercatorLatMaxDeg || bounds.MaxLatDeg >= kMercatorLatMaxDeg) {
    return std::unexpected(RangeError::InvalidRequest);
  }
  const auto first = ::outshine::Ground::ToTileFracClamped(
      {.LongitudeDeg = bounds.MinLonDeg, .LatitudeDeg = bounds.MaxLatDeg}, zoom);
  const auto last = ::outshine::Ground::ToTileFracClamped(
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

std::expected<void, RangeError> IncludeRange(const TerrainFieldSet::Entry &source,
                                             int zoom,
                                             SampleRectangle rectangle,
                                             size_t nodesMost,
                                             TerrainFieldSet::HeightRange &range) {
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

}

bool TerrainFieldSet::ShareSourcedField(Data::TileId tile,
                                        ::outshine::Ground::HeightField::Block &into) const {
  return Share(Fields_, tile, into);
}

bool TerrainFieldSet::Share(std::span<const Entry> fields,
                            Data::TileId tile,
                            ::outshine::Ground::HeightField::Block &into) {
  if (tile.Zoom < 0 || tile.Zoom > ::outshine::Ground::HeightField::MaximumTileZoom) {
    return false;
  }
  const auto found = SourceFor(fields, tile);
  if (found == fields.end()) { return false; }
  return found->first == tile
             ? ::outshine::Ground::HeightField::SharesField(found->second, tile, into)
             : ::outshine::Ground::HeightField::ResamplesSourcedAncestor(
                   *found->second, found->first, tile, into);
}

bool TerrainFieldSet::Copy(std::span<const Entry> fields,
                           Data::TileId tile,
                           ::outshine::Ground::HeightField::Block &into) {
  if (tile.Zoom < 0 || tile.Zoom > ::outshine::Ground::HeightField::MaximumTileZoom) {
    return false;
  }
  const auto found = SourceFor(fields, tile);
  if (found == fields.end()) { return false; }
  if (found->first == tile) {
    return ::outshine::Ground::HeightField::CopiesField(*found->second, tile, into);
  }
  return ::outshine::Ground::HeightField::ResamplesSourcedAncestor(
      *found->second, found->first, tile, into);
}

bool TerrainFieldSet::CopySourcedField(Data::TileId tile,
                                       ::outshine::Ground::HeightField::Block &into) const {
  return Copy(Fields_, tile, into);
}

std::optional<double> TerrainFieldSet::AslMAt(int zoom, LongitudeLatitude at) const {
  return AslMAt(Fields_, zoom, at);
}

std::optional<double>
TerrainFieldSet::AslMAt(std::span<const Entry> fields, int zoom, LongitudeLatitude at) {
  for (int heldZoom = zoom; heldZoom >= 0; --heldZoom) {
    const ::outshine::Ground::TileFrac frac = ::outshine::Ground::ToTileFracClamped(
        {.LongitudeDeg = at.LongitudeDeg, .LatitudeDeg = at.LatitudeDeg}, heldZoom);
    const Data::TileId tile{.Zoom = heldZoom,
                            .X = static_cast<uint32_t>(std::floor(frac.X)),
                            .Y = static_cast<uint32_t>(std::floor(frac.Y))};
    const auto found =
        std::ranges::find_if(fields, [tile](const Entry &entry) { return entry.first == tile; });
    const ::outshine::Ground::TerrainField *field =
        found == fields.end() ? nullptr : found->second.get();
    if (field == nullptr || !field->Meshable()) { continue; }
    return field->PostingM(
        {.Col = frac.X - std::floor(frac.X), .Row = frac.Y - std::floor(frac.Y)});
  }
  return std::nullopt;
}

std::expected<TerrainFieldSet::HeightRange, TerrainFieldSet::RangeError>
TerrainFieldSet::AslRangeOver(int zoom,
                              ::outshine::Ground::GeoBounds bounds,
                              size_t nodesMost) const {
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
