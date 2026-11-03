#ifndef OUTSHINE_ENGINE_STREAMING_TERRAINCLIPMAPPREPARATION_H
#define OUTSHINE_ENGINE_STREAMING_TERRAINCLIPMAPPREPARATION_H
#include "Vec2.h"
#include <cstddef>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include "ClipmapHeightCache.h"

namespace outshine {
class Tasks;
class TerrainSurfacePublication;
class TerrainClipmapRequest;

namespace Render {
class ClipmapSurface;
}

struct TerrainClipmapAdmission {
  size_t WorkBytesMost = 0;
  size_t StagingBytesMost = 0;
  size_t SamplesPerSlice = 0;
};

class TerrainClipmapPreparation {
public:
  TerrainClipmapPreparation(Tasks &worker,
                            std::shared_ptr<const TerrainSurfacePublication> source,
                            std::shared_ptr<Render::ClipmapSurface> surface);
  ~TerrainClipmapPreparation();
  [[nodiscard]] std::expected<void, std::string> Advance(Vec2 camera,
                                                         TerrainClipmapAdmission admission);
  [[nodiscard]] bool Ready() const noexcept;
  [[nodiscard]] size_t HeapBytes() const noexcept;

private:
  Tasks *Worker_;
  std::shared_ptr<const TerrainSurfacePublication> Source_;
  std::shared_ptr<Render::ClipmapSurface> Surface_;
  std::unique_ptr<TerrainClipmapRequest> Heights_, Normals_;
  std::optional<Render::ClipmapHeightUpdate> PreparedHeights_, PreparedNormals_;
};
}
#endif
