#include "SourcedTerrainFields.h"

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <optional>
#include <ranges>
#include <span>

namespace outshine {

bool SourcedTerrainFields::Copy(std::span<const Entry> fields,
                                Data::TileId tile,
                                Ground::HeightField::Block &into) {
  if (tile.Zoom < 0 || tile.Zoom > Ground::HeightField::MaximumTileZoom) { return false; }
  for (int zoom = tile.Zoom; zoom >= 0; --zoom) {
    const auto drop = static_cast<uint32_t>(tile.Zoom - zoom);
    const Data::TileId source{.Zoom = zoom, .X = tile.X >> drop, .Y = tile.Y >> drop};
    const auto found = std::ranges::find_if(
        fields, [source](const Entry &entry) { return entry.first == source; });
    const Ground::TerrainField *const field = found == fields.end() ? nullptr : found->second.get();
    if (field == nullptr || !field->Meshable() || field->Sources().empty()) { continue; }
    if (zoom == tile.Zoom) { return Ground::HeightField::CopiesField(*field, tile, into); }
    return Ground::HeightField::ResamplesSourcedAncestor(*field, source, tile, into);
  }
  return false;
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
