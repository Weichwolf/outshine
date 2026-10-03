#ifndef OUTSHINE_WORLD_GROUND_TERRAINDELIVERY_H
#define OUTSHINE_WORLD_GROUND_TERRAINDELIVERY_H

#include "TilePool.h"
#include "tiles/TerrainTiles.h"
#include <world/data/Source.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace outshine::Ground {

[[nodiscard]] inline TerrainBytes FromTerrainDelivery(const Data::Fetch &request,
                                                      TilePool::Landing landing,
                                                      const Data::Source *source = nullptr) {
  const auto tile = landing.At.Tile();
  const auto corrupt = [&] {
    return TerrainBytes::Wire(
        Data::FetchFailure{.Kind = request.Kind(),
                           .Requested = request.Where(),
                           .Served = landing.At,
                           .SourceId = std::move(landing.SourceId),
                           .SourceRevision = std::move(landing.SourceRevision),
                           .SourceKey = std::move(landing.SourceKey),
                           .Reason = Data::FetchFailureReason::CorruptPayload});
  };
  if (!tile) { return corrupt(); }
  if (source != nullptr) {
    auto decoded = source->DecodeElevation(landing.Bytes);
    if (decoded) {
      const uint64_t samples = static_cast<uint64_t>(decoded->Rows) * decoded->Cols;
      if (decoded->Rows < 2 || decoded->Cols < 2 || samples != decoded->Meters.size() ||
          !std::ranges::all_of(
              decoded->Meters, [](float height) { return std::isfinite(height); })) {
        return corrupt();
      }
      return TerrainBytes::From(*tile,
                                TerrainField(std::move(*decoded)),
                                {.Kind = request.Kind(),
                                 .Tile = *tile,
                                 .SourceId = std::move(landing.SourceId),
                                 .Revision = std::move(landing.SourceRevision)},
                                std::move(landing.SourceKey),
                                std::move(landing.TerrainStamp));
    }
    if (decoded.error() != Data::DecodeFailure::Unsupported) { return corrupt(); }
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
