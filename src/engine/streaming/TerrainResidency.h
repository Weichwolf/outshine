#ifndef OUTSHINE_ENGINE_STREAMING_TERRAINRESIDENCY_H
#define OUTSHINE_ENGINE_STREAMING_TERRAINRESIDENCY_H

#include "Address.h"
#include "FlatMap.h"
#include "GroundMesher.h"
#include "TangentFrame.h"
#include "scene/TerrainTile.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <vector>

namespace outshine {

namespace Render {
class SceneRenderer;
}

class TerrainResidency {
public:
  TerrainResidency() = default;
  TerrainResidency(const TerrainResidency &other);
  TerrainResidency &operator=(const TerrainResidency &) = delete;
  TerrainResidency(TerrainResidency &&) noexcept = default;
  TerrainResidency &operator=(TerrainResidency &&) noexcept = default;

  void Into(Render::SceneRenderer *renderer) noexcept { Renderer_ = renderer; }

  [[nodiscard]] bool
  Publish(const Patchwork &patchwork, const TangentFrame &frame, std::string &error);
  [[nodiscard]] bool BeginPublish(const Patchwork &patchwork, std::string &error);
  [[nodiscard]] std::expected<bool, std::string>
  AdvancePublish(const Patchwork &patchwork, const TangentFrame &frame, size_t sheetsMost);

  void Clear();

  [[nodiscard]] size_t Standing() const noexcept { return Held_.size(); }

  [[nodiscard]] size_t Instances() const noexcept { return Instances_.size() + Virtual_.size(); }

  [[nodiscard]] size_t Flat() const noexcept { return Flat_; }

  [[nodiscard]] double LongestBatchMs() const noexcept { return LongestBatchMs_; }

  [[nodiscard]] double FinalizeMs() const noexcept { return FinalizeMs_; }

  [[nodiscard]] uint64_t Digest() const noexcept;
  [[nodiscard]] size_t HeapBytes() const noexcept;

private:
  struct TileHash {
    [[nodiscard]] uint64_t operator()(const Data::TileId &tile) const noexcept;
  };

  struct Held {
    Data::TileId Tile;
    Render::HeightPageHandle Page{};
    std::vector<float> Nodes;
  };

  [[nodiscard]] std::expected<Render::HeightPageHandle, std::string>
  PageFor(Data::TileId tile, std::span<const float> nodes);
  [[nodiscard]] static Render::TerrainTile TileOf(Data::TileId tile,
                                                  Render::HeightPageHandle page,
                                                  std::span<const float> nodes,
                                                  const TangentFrame &frame);
  [[nodiscard]] bool PublishGrid(const Patchwork &patchwork, std::string &error);
  [[nodiscard]] bool ReindexPages(std::string &error);

  std::vector<Held> Held_;
  FlatMap<size_t, Data::TileId, TileHash> PageIndex_;
  std::vector<Render::TerrainTile> Instances_;
  std::vector<Render::TerrainTile> Virtual_;
  size_t Flat_ = 0;
  uint32_t GridPostings_ = 0;
  Render::SceneRenderer *Renderer_ = nullptr;
  size_t NextSheet_ = 0;
  bool Publishing_ = false;
  double LongestBatchMs_ = 0.0;
  double FinalizeMs_ = 0.0;
};

}
#endif
