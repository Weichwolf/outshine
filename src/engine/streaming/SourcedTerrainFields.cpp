#include "SourcedTerrainFields.h"

#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <expected>
#include <cmath>
#include <optional>
#include <ranges>
#include <span>

namespace outshine {
namespace {

std::span<const SourcedTerrainFields::Entry>::iterator
SourceFor(std::span<const SourcedTerrainFields::Entry> fields, Data::TileId tile) {
  for (int zoom = tile.Zoom; zoom >= 0; --zoom) {
    const auto drop = static_cast<uint32_t>(tile.Zoom - zoom);
    const Data::TileId source{.Zoom = zoom, .X = tile.X >> drop, .Y = tile.Y >> drop};
    const auto found =
        std::ranges::find_if(fields, [source](const auto &entry) { return entry.first == source; });
    if (found != fields.end() && found->second && found->second->Meshable() &&
        !found->second->Sources().empty()) {
      return found;
    }
  }
  return fields.end();
}

}

size_t SourcedTerrainFields::RetainedBytes() const noexcept {
  size_t bytes = Fields_.capacity() * sizeof(Entry);
  for (auto at = Fields_.begin(); at != Fields_.end(); ++at) {
    if (at->second && std::none_of(Fields_.begin(), at, [&at](const Entry &entry) {
          return entry.second == at->second;
        })) {
      bytes += at->second->HeapBytes();
    }
  }
  return bytes;
}

std::expected<SourcedTerrainFields, SourcedTerrainFields::CaptureError>
SourcedTerrainFields::Capture(std::span<const Entry> fields,
                              std::span<const Ground::TileSpot> requests,
                              size_t bytesMost) {
  if (requests.size() > bytesMost / sizeof(Entry)) {
    return std::unexpected(CaptureError::OverBudget);
  }
  SourcedTerrainFields captured;
  captured.Fields_.reserve(requests.size());
  size_t bytes = captured.Fields_.capacity() * sizeof(Entry);
  if (bytes > bytesMost) { return std::unexpected(CaptureError::OverBudget); }
  for (const Ground::TileSpot request : requests) {
    if (request.Zoom < 0 || request.Zoom > Ground::HeightField::MaximumTileZoom || request.X < 0 ||
        request.Y < 0 ||
        static_cast<uint64_t>(request.X) >= (uint64_t{1} << static_cast<uint32_t>(request.Zoom)) ||
        static_cast<uint64_t>(request.Y) >= (uint64_t{1} << static_cast<uint32_t>(request.Zoom))) {
      return std::unexpected(CaptureError::InvalidRequest);
    }
    const auto found = SourceFor(fields,
                                 {.Zoom = request.Zoom,
                                  .X = static_cast<uint32_t>(request.X),
                                  .Y = static_cast<uint32_t>(request.Y)});
    if (found == fields.end()) { return std::unexpected(CaptureError::MissingSource); }
    if (std::ranges::any_of(captured.Fields_,
                            [&found](const Entry &entry) { return entry.first == found->first; })) {
      continue;
    }
    const bool held = std::ranges::any_of(
        captured.Fields_, [&found](const Entry &entry) { return entry.second == found->second; });
    const size_t added = held ? 0 : found->second->HeapBytes();
    if (added > bytesMost - bytes) { return std::unexpected(CaptureError::OverBudget); }
    bytes += added;
    captured.Fields_.push_back(*found);
  }
  return captured;
}

bool SourcedTerrainFields::FitsPreparation(std::span<const Ground::TileSpot> requests,
                                           size_t bytesMost) const {
  size_t bytes = RetainedBytes();
  const auto charge = [&bytes, bytesMost](size_t count, size_t size) {
    if (size == 0) { return true; }
    if (bytes > bytesMost || count > (bytesMost - bytes) / size) { return false; }
    bytes += count * size;
    return true;
  };
  if (!charge(1, sizeof(Ground::HeightField)) ||
      !charge(requests.size(), sizeof(Ground::TileSpot) + sizeof(Ground::HeightField::Block))) {
    return false;
  }
  for (const auto request : requests) {
    if (request.Zoom < 0 || request.Zoom > Ground::HeightField::MaximumTileZoom || request.X < 0 ||
        request.Y < 0 ||
        static_cast<uint64_t>(request.X) >= (uint64_t{1} << static_cast<uint32_t>(request.Zoom)) ||
        static_cast<uint64_t>(request.Y) >= (uint64_t{1} << static_cast<uint32_t>(request.Zoom))) {
      return false;
    }
    const Data::TileId tile{.Zoom = request.Zoom,
                            .X = static_cast<uint32_t>(request.X),
                            .Y = static_cast<uint32_t>(request.Y)};
    const auto found = SourceFor(Fields_, tile);
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
    if (found->first != tile) {
      const auto scale = uint64_t{1} << static_cast<uint32_t>(tile.Zoom - found->first.Zoom);
      const size_t side = std::min<size_t>((field.Cols() - 1u + scale - 1u) / scale + 1u, 257u);
      if (!charge(side * side, sizeof(float))) { return false; }
    }
  }
  return true;
}

bool SourcedTerrainFields::ShareSourcedField(Data::TileId tile,
                                             Ground::HeightField::Block &into) const {
  if (tile.Zoom < 0 || tile.Zoom > Ground::HeightField::MaximumTileZoom) { return false; }
  const auto found = SourceFor(Fields_, tile);
  if (found == Fields_.end()) { return false; }
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

}
