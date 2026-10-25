#ifndef OUTSHINE_SOURCES_ADDRESS_H
#define OUTSHINE_SOURCES_ADDRESS_H

#include "GeoCellId.h"
#include "TileId.h"
#include <cstdint>
#include <optional>
#include <string>
#include <variant>

namespace outshine::Data {

enum class Scheme : uint8_t { TileZxy, WholeWorld, GeographicCell, GeodeticGrid };

struct CellId {
  int SouthDeg = 0;
  int WestDeg = 0;

  [[nodiscard]] bool operator==(const CellId &) const noexcept = default;
};

class Address {
public:
  static Address At(TileId tile) { return Address(tile); }

  static Address Whole(uint32_t index) { return Address(index); }

  static Address AtCell(CellId cell) { return Address(cell); }

  static Address AtGeoCell(GeoCellId cell) { return Address(cell); }

  [[nodiscard]] Scheme How() const noexcept {
    if (std::holds_alternative<TileId>(Held_)) { return Scheme::TileZxy; }
    if (std::holds_alternative<CellId>(Held_)) { return Scheme::GeographicCell; }
    return std::holds_alternative<GeoCellId>(Held_) ? Scheme::GeodeticGrid : Scheme::WholeWorld;
  }

  [[nodiscard]] std::optional<TileId> Tile() const noexcept {
    if (const auto *tile = std::get_if<TileId>(&Held_)) { return *tile; }
    return std::nullopt;
  }

  [[nodiscard]] std::optional<uint32_t> Index() const noexcept {
    if (const auto *index = std::get_if<uint32_t>(&Held_)) { return *index; }
    return std::nullopt;
  }

  [[nodiscard]] std::optional<CellId> Cell() const noexcept {
    if (const auto *cell = std::get_if<CellId>(&Held_)) { return *cell; }
    return std::nullopt;
  }

  [[nodiscard]] std::optional<GeoCellId> GeoCell() const noexcept {
    if (const auto *cell = std::get_if<GeoCellId>(&Held_)) { return *cell; }
    return std::nullopt;
  }

  [[nodiscard]] std::string Text() const;

  [[nodiscard]] bool operator==(const Address &o) const noexcept { return Held_ == o.Held_; }

  [[nodiscard]] bool operator!=(const Address &o) const noexcept { return !(*this == o); }

private:
  explicit Address(TileId tile) : Held_(tile) {}

  explicit Address(uint32_t index) : Held_(index) {}

  explicit Address(CellId cell) : Held_(cell) {}

  explicit Address(GeoCellId cell) : Held_(cell) {}

  std::variant<TileId, uint32_t, CellId, GeoCellId> Held_;
};

}
#endif
