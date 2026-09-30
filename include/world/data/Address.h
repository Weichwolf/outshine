#ifndef OUTSHINE_WORLD_DATA_ADDRESS_H
#define OUTSHINE_WORLD_DATA_ADDRESS_H

#include <cstdint>
#include <optional>
#include <string>

namespace outshine::Data {

/// Address family; source addresses remain separate from render ownership.
enum class Scheme : uint8_t {
  TileZxy,   ///< Web-Mercator tile coordinates.
  WholeWorld ///< Source-defined indexed whole-domain products.
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

/// Value-only source address. Construction does not validate source coverage.
/// Copying retains no resources or owners; all operations except Text are constant-time.
class Address {
public:
  /// Construct a tile address without allocation.
  /// @param tile Coordinates interpreted and validated by the source.
  /// @return Value owning those coordinates.
  static Address At(TileId tile) { return {Scheme::TileZxy, tile}; }

  /// Construct an indexed whole-domain product address without allocation.
  /// @param index Source-defined product index.
  /// @return Value owning that index.
  static Address Whole(uint32_t index) {
    return {Scheme::WholeWorld, TileId{.Zoom = 0, .X = index, .Y = 0}};
  }

  /// Inspect the address family.
  /// @return Stored scheme; no allocation.
  [[nodiscard]] Scheme How() const noexcept { return How_; }

  /// Read coordinates only when this is a tile address.
  /// @return Copied tile ID or nothing; no allocation.
  [[nodiscard]] std::optional<TileId> Tile() const noexcept {
    if (How_ != Scheme::TileZxy) { return std::nullopt; }
    return Held_;
  }

  /// Read an index only for the whole-domain scheme.
  /// @return Copied product index or nothing; no allocation.
  [[nodiscard]] std::optional<uint32_t> Index() const noexcept {
    if (How_ != Scheme::WholeWorld) { return std::nullopt; }
    return Held_.X;
  }

  /// Serialize as z/x/y or w/index for identity and diagnostics.
  /// @return Owned string; may allocate.
  [[nodiscard]] std::string Text() const;

  /// Compare scheme and stored coordinates exactly.
  /// @param o Borrowed comparison value.
  /// @return True for identical addresses; no allocation.
  [[nodiscard]] bool operator==(const Address &o) const noexcept {
    return How_ == o.How_ && Held_ == o.Held_;
  }

  /// Compare scheme and stored coordinates for inequality.
  /// @param o Borrowed comparison value.
  /// @return True for different addresses; no allocation.
  [[nodiscard]] bool operator!=(const Address &o) const noexcept { return !(*this == o); }

private:
  Address(Scheme how, TileId held) : How_(how), Held_(held) {}

  Scheme How_;
  TileId Held_;
};

}
#endif
