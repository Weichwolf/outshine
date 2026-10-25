#ifndef OUTSHINE_GENERATORS_TERRAIN_INPUTS_COPERNICUSTERRAIN_H
#define OUTSHINE_GENERATORS_TERRAIN_INPUTS_COPERNICUSTERRAIN_H

#include <cstddef>
#include <memory>
#include <optional>
#include <SourceRequest.h>
#include "TerrainTiles.h"

namespace outshine::Ground {

class TilePool;

class CopernicusTerrain {
public:
  explicit CopernicusTerrain(TilePool &pool);
  ~CopernicusTerrain();

  [[nodiscard]] TerrainBytes Take(Data::TileId at);
  [[nodiscard]] const std::optional<Data::SourceRequest> &Awaiting() const noexcept;
  [[nodiscard]] size_t HeapBytes() const noexcept;

private:
  struct Impl;
  std::unique_ptr<Impl> State_;
};

}
#endif
