#ifndef OUTSHINE_WORLD_GROUND_TERRAINDELIVERY_H
#define OUTSHINE_WORLD_GROUND_TERRAINDELIVERY_H

#include "TilePool.h"
#include "tiles/TerrainTiles.h"
#include <utility>

namespace outshine::Ground {

[[nodiscard]] inline TerrainBytes FromTerrainDelivery(const Data::Fetch &request,
                                                      TilePool::Landing landing) {
  const auto tile = landing.At.Tile();
  if (!tile) {
    return TerrainBytes::Wire(
        Data::FetchFailure{.Kind = request.Kind(),
                           .Requested = request.Where(),
                           .Served = landing.At,
                           .SourceId = std::move(landing.SourceId),
                           .SourceRevision = std::move(landing.SourceRevision),
                           .SourceKey = std::move(landing.SourceKey),
                           .Reason = Data::FetchFailureReason::CorruptPayload});
  }
  return TerrainBytes::From(*tile,
                            std::move(landing.Bytes),
                            {.Kind = request.Kind(),
                             .Tile = *tile,
                             .SourceId = std::move(landing.SourceId),
                             .Revision = std::move(landing.SourceRevision)},
                            std::move(landing.SourceKey),
                            std::move(landing.TerrainStamp));
}

}
#endif
