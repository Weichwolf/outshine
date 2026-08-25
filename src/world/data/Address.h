#ifndef OUTSHINE_WORLD_DATA_ADDRESS_H
#define OUTSHINE_WORLD_DATA_ADDRESS_H

#include <cstdint>
#include <string>

namespace outshine::Data {

enum class Scheme : uint8_t { TileZxy, WholeWorld };

class Address {
public:
  static Address Tile(int z, uint32_t x, uint32_t y) { return Address(Scheme::TileZxy, z, x, y); }
  static Address Whole(uint32_t index) { return Address(Scheme::WholeWorld, 0, index, 0); }

  [[nodiscard]] Scheme How() const noexcept { return How_; }

  [[nodiscard]] bool TryTile(int *z, uint32_t *x, uint32_t *y) const noexcept {
    if (How_ != Scheme::TileZxy) return false;
    *z = Z_;
    *x = X_;
    *y = Y_;
    return true;
  }
  [[nodiscard]] bool TryIndex(uint32_t *index) const noexcept {
    if (How_ != Scheme::WholeWorld) return false;
    *index = X_;
    return true;
  }

  [[nodiscard]] std::string Text() const;

  [[nodiscard]] bool operator==(const Address &o) const noexcept {
    return How_ == o.How_ && Z_ == o.Z_ && X_ == o.X_ && Y_ == o.Y_;
  }
  [[nodiscard]] bool operator!=(const Address &o) const noexcept { return !(*this == o); }

private:
  Address(Scheme how, int z, uint32_t x, uint32_t y) : How_(how), Z_(z), X_(x), Y_(y) {}

  Scheme How_;
  int Z_;
  uint32_t X_, Y_;
};

}
#endif
