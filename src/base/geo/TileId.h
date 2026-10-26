#ifndef OUTSHINE_BASE_GEO_TILEID_H
#define OUTSHINE_BASE_GEO_TILEID_H

#include <cstdint>

namespace outshine::Data {

struct TileId {

  static constexpr int MaximumZoom = 30;

  int Zoom = 0;

  uint32_t X = 0;

  uint32_t Y = 0;

  [[nodiscard]] bool operator==(const TileId &) const noexcept = default;
};

}
#endif
