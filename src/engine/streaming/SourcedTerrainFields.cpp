#include "SourcedTerrainFields.h"

#include <algorithm>
#include <cstdint>
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

}
