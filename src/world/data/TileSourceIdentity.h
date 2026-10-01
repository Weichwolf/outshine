#ifndef OUTSHINE_WORLD_DATA_TILESOURCEIDENTITY_H
#define OUTSHINE_WORLD_DATA_TILESOURCEIDENTITY_H

#include <compare>
#include <optional>
#include <string>
#include <tuple>

#include <world/data/Address.h>
#include <world/data/DataKind.h>

namespace outshine::Data {

struct TileSourceIdentity {
  enum class Origin { Provider, Direct, Declared, Shaped };

  Origin From = Origin::Provider;
  DataKind Kind = DataKind::Elevation;
  TileId Tile{};
  std::optional<CellId> NativeCell = std::nullopt;
  std::string SourceId;
  std::string Revision;

  [[nodiscard]] bool operator==(const TileSourceIdentity &) const noexcept = default;

  [[nodiscard]] std::strong_ordering operator<=>(const TileSourceIdentity &other) const {
    const bool native = NativeCell.has_value();
    const bool otherNative = other.NativeCell.has_value();
    const CellId cell = NativeCell.value_or(CellId{});
    const CellId otherCell = other.NativeCell.value_or(CellId{});
    return std::tie(From,
                    Kind,
                    native,
                    cell.SouthDeg,
                    cell.WestDeg,
                    Tile.Zoom,
                    Tile.X,
                    Tile.Y,
                    SourceId,
                    Revision) <=> std::tie(other.From,
                                           other.Kind,
                                           otherNative,
                                           otherCell.SouthDeg,
                                           otherCell.WestDeg,
                                           other.Tile.Zoom,
                                           other.Tile.X,
                                           other.Tile.Y,
                                           other.SourceId,
                                           other.Revision);
  }
};

}
#endif
