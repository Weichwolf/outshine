#ifndef OUTSHINE_WORLD_DATA_TILESOURCEIDENTITY_H
#define OUTSHINE_WORLD_DATA_TILESOURCEIDENTITY_H

#include <compare>
#include <string>
#include <tuple>

#include "Address.h"
#include "DataKind.h"

namespace outshine::Data {

struct TileSourceIdentity {
  enum class Origin { Provider, Direct, Declared, Shaped };

  Origin From = Origin::Provider;
  DataKind Kind = DataKind::Elevation;
  TileId Tile;
  std::string SourceId;
  std::string Revision;

  [[nodiscard]] bool operator==(const TileSourceIdentity &) const noexcept = default;

  [[nodiscard]] std::strong_ordering operator<=>(const TileSourceIdentity &other) const {
    return std::tie(From, Kind, Tile.Zoom, Tile.X, Tile.Y, SourceId, Revision) <=>
           std::tie(other.From,
                    other.Kind,
                    other.Tile.Zoom,
                    other.Tile.X,
                    other.Tile.Y,
                    other.SourceId,
                    other.Revision);
  }
};

}
#endif
