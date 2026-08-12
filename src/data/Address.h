#ifndef ADDRESS_H
#define ADDRESS_H

#include <cstdint>
#include <string>

namespace outshine::Data {

/* HOW A SOURCE IS ASKED, and there is more than one arm from the first day because there is more
 * than one live upstream shape: a slippy tile is z/x/y, and the star catalogue is one whole-world
 * blob per magnitude band. A design that assumed z/x/y everywhere would have to be reopened to admit
 * the second, and a bounding-box query is the next arm rather than a new interface. */
enum class Scheme : uint8_t { TileZxy, WholeWorld };

class Address {
public:
  static Address Tile(int z, uint32_t x, uint32_t y) { return Address(Scheme::TileZxy, z, x, y); }
  static Address Whole(uint32_t index) { return Address(Scheme::WholeWorld, 0, index, 0); }

  [[nodiscard]] Scheme How() const noexcept { return How_; }

  /* Written only for a tile address; the three numbers have no meaning under the other arm. */
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

  /* THE CANONICAL TEXT, and both keys in this layer are built over it: the request key the byte
   * cache holds, and the content key the store names a file by. One spelling, so a cache hit and a
   * store hit cannot disagree about what was asked. */
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

} // namespace outshine::Data
#endif
