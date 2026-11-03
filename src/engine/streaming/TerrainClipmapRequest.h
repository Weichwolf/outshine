#ifndef OUTSHINE_ENGINE_STREAMING_TERRAINCLIPMAPREQUEST_H
#define OUTSHINE_ENGINE_STREAMING_TERRAINCLIPMAPREQUEST_H

#include "ClipmapHeightCache.h"
#include "TerrainSurfacePublication.h"
#include "Tasks.h"
#include <cstddef>
#include <expected>
#include <memory>
#include <optional>
#include <string>

namespace outshine {
struct TerrainClipmapSampling {
  double SpacingM = 1;
  size_t SamplesPerSlice = 0;
};

class TerrainClipmapRequest {
public:
  using Result = std::expected<Render::ClipmapHeightUpdate, std::string>;

  TerrainClipmapRequest(Tasks &pool,
                        std::shared_ptr<const TerrainSurfacePublication> source,
                        Render::ClipmapHeightUpdate update,
                        TerrainClipmapSampling sampling);
  ~TerrainClipmapRequest();
  TerrainClipmapRequest(const TerrainClipmapRequest &) = delete;
  TerrainClipmapRequest &operator=(const TerrainClipmapRequest &) = delete;

  [[nodiscard]] bool Finished() const noexcept;
  [[nodiscard]] std::optional<Result> Take();
  [[nodiscard]] size_t HeapBytes() const noexcept;

private:
  struct State;
  std::shared_ptr<State> State_;
};
}
#endif
