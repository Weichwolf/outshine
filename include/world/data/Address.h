#ifndef OUTSHINE_WORLD_DATA_ADDRESS_H
#define OUTSHINE_WORLD_DATA_ADDRESS_H

#include <cstdint>
#include <optional>
#include <string>
#include <variant>

namespace outshine::Data {

/// Address family; source addresses remain separate from render ownership.
enum class Scheme : uint8_t {
  TileZxy,       ///< Web-Mercator tile coordinates.
  WholeWorld,    ///< Source-defined indexed whole-domain products.
  GeographicCell ///< One-degree WGS84 source cell, independent of Mercator render tiles.
};

/// Web-Mercator z/x/y address. Sources validate zoom and coordinates before use.
struct TileId {
  /// Largest supported zoom; each axis then has 2^MaximumZoom cells.
  static constexpr int MaximumZoom = 30;
  /// Zoom in [0, MaximumZoom].
  int Zoom = 0;
  /// Column in [0, 2^Zoom).
  uint32_t X = 0;
  /// Row in [0, 2^Zoom).
  uint32_t Y = 0;

  /// Compare value coordinates without allocation.
  /// @return True for identical zoom, row and column.
  [[nodiscard]] bool operator==(const TileId &) const noexcept = default;
};

/// One-degree WGS84 cell named by its southwest corner; sources validate coverage.
struct CellId {
  int SouthDeg = 0; ///< Latitude in [-90, 89]; the north edge is SouthDeg + 1.
  int WestDeg = 0;  ///< Longitude in [-180, 179]; the east edge is WestDeg + 1.

  /// Compare native source coordinates without allocation.
  /// @return True for identical cell corners.
  [[nodiscard]] bool operator==(const CellId &) const noexcept = default;
};

/// Value-only source address. Construction does not validate source coverage.
/// Copying retains no resources or owners; all operations except Text are constant-time.
class Address {
public:
  /// Construct a tile address without allocation.
  /// @param tile Coordinates interpreted and validated by the source.
  /// @return Value owning those coordinates.
  static Address At(TileId tile) { return Address(tile); }

  /// Construct an indexed whole-domain product address without allocation.
  /// @param index Source-defined product index.
  /// @return Value owning that index.
  static Address Whole(uint32_t index) { return Address(index); }

  /// Construct a native geographic source address without allocation.
  /// @param cell Integer southwest corner; construction does not validate coverage.
  /// @return Value owning the geographic cell coordinates.
  static Address AtCell(CellId cell) { return Address(cell); }

  /// Inspect the address family.
  /// @return Stored scheme; no allocation.
  [[nodiscard]] Scheme How() const noexcept {
    if (std::holds_alternative<TileId>(Held_)) { return Scheme::TileZxy; }
    return std::holds_alternative<CellId>(Held_) ? Scheme::GeographicCell : Scheme::WholeWorld;
  }

  /// Read coordinates only when this is a tile address.
  /// @return Copied tile ID or nothing; no allocation.
  [[nodiscard]] std::optional<TileId> Tile() const noexcept {
    if (const auto *tile = std::get_if<TileId>(&Held_)) { return *tile; }
    return std::nullopt;
  }

  /// Read an index only for the whole-domain scheme.
  /// @return Copied product index or nothing; no allocation.
  [[nodiscard]] std::optional<uint32_t> Index() const noexcept {
    if (const auto *index = std::get_if<uint32_t>(&Held_)) { return *index; }
    return std::nullopt;
  }

  /// Read the southwest corner only for a geographic source address.
  /// @return Copied cell ID or nothing; no allocation.
  [[nodiscard]] std::optional<CellId> Cell() const noexcept {
    if (const auto *cell = std::get_if<CellId>(&Held_)) { return *cell; }
    return std::nullopt;
  }

  /// Serialize as z/x/y, w/index or g/south/west for identity and diagnostics.
  /// @return Owned string; may allocate.
  [[nodiscard]] std::string Text() const;

  /// Compare scheme and stored coordinates exactly.
  /// @param o Borrowed comparison value.
  /// @return True for identical addresses; no allocation.
  [[nodiscard]] bool operator==(const Address &o) const noexcept { return Held_ == o.Held_; }

  /// Compare scheme and stored coordinates for inequality.
  /// @param o Borrowed comparison value.
  /// @return True for different addresses; no allocation.
  [[nodiscard]] bool operator!=(const Address &o) const noexcept { return !(*this == o); }

private:
  explicit Address(TileId tile) : Held_(tile) {}

  explicit Address(uint32_t index) : Held_(index) {}

  explicit Address(CellId cell) : Held_(cell) {}

  std::variant<TileId, uint32_t, CellId> Held_;
};

}
#endif
