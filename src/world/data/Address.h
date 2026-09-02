#ifndef OUTSHINE_WORLD_DATA_ADDRESS_H
#define OUTSHINE_WORLD_DATA_ADDRESS_H

#include <cstdint>
#include <optional>
#include <string>

namespace outshine::Data {

enum class Scheme : uint8_t { TileZxy, WholeWorld };

struct TileId {
  int Zoom = 0;
  uint32_t X = 0;
  uint32_t Y = 0;

  [[nodiscard]] bool operator==(const TileId &) const noexcept = default;
};

class Address {
public:
  static Address At(TileId tile) { return {Scheme::TileZxy, tile}; }

  static Address Whole(uint32_t index) {
    return {Scheme::WholeWorld, TileId{.Zoom = 0, .X = index, .Y = 0}};
  }

  [[nodiscard]] Scheme How() const noexcept { return How_; }

  [[nodiscard]] std::optional<TileId> Tile() const noexcept {
    if (How_ != Scheme::TileZxy) { return std::nullopt; }
    return Held_;
  }

  [[nodiscard]] std::optional<uint32_t> Index() const noexcept {
    if (How_ != Scheme::WholeWorld) { return std::nullopt; }
    return Held_.X;
  }

  [[nodiscard]] std::string Text() const;

  [[nodiscard]] bool operator==(const Address &o) const noexcept {
    return How_ == o.How_ && Held_ == o.Held_;
  }

  [[nodiscard]] bool operator!=(const Address &o) const noexcept { return !(*this == o); }

private:
  Address(Scheme how, TileId held) : How_(how), Held_(held) {}

  Scheme How_;
  TileId Held_;
};

} // namespace outshine::Data
#endif
