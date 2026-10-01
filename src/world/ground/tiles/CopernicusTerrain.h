#ifndef OUTSHINE_WORLD_GROUND_TILES_COPERNICUSTERRAIN_H
#define OUTSHINE_WORLD_GROUND_TILES_COPERNICUSTERRAIN_H

#include <cstddef>
#include <memory>
#include <optional>
#include <world/data/Fetch.h>
#include "TerrainTiles.h"

namespace outshine::Ground {

class TilePool;

class CopernicusTerrain {
public:
  explicit CopernicusTerrain(TilePool &pool);
  ~CopernicusTerrain();

  [[nodiscard]] TerrainBytes Take(Data::TileId at);
  [[nodiscard]] const std::optional<Data::Fetch> &Awaiting() const noexcept;
  [[nodiscard]] size_t HeapBytes() const noexcept;

private:
  struct Impl;
  std::unique_ptr<Impl> State_;
};

}
#endif
