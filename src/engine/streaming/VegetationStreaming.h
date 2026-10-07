#ifndef OUTSHINE_ENGINE_STREAMING_VEGETATIONSTREAMING_H
#define OUTSHINE_ENGINE_STREAMING_VEGETATIONSTREAMING_H

#include "PreparedImpostorAssets.h"
#include <optional>
#include "ImpostorAtlasShape.h"
#include "ImpostorInstances.h"
#include "WorldPlacement.h"
#include "Shipped.h"

namespace outshine {
class VegetationStreaming {
public:
  enum class ResourcePublication : uint8_t { Deferred, Allowed };

  struct Config {
    Content::PreparedImpostorAssets::Config Cache;
    Content::ImpostorAtlasShape Shape;
    size_t Prototypes = 64;
    size_t Instances = 65536;
  };

  struct Counters {
    Content::PreparedImpostorAssets::Counters Assets;
    uint64_t Generated = 0;
    double GenerationMs = 0.0;
  };

  static std::unique_ptr<VegetationStreaming> Create(Render::SceneRenderer &renderer,
                                                     Tasks &compute,
                                                     const Generators::Shipping &catalogue,
                                                     std::span<const WorldInstance> instances,
                                                     const TangentFrame &frame,
                                                     const Config &config,
                                                     std::string &error);
  ~VegetationStreaming();
  void Into(Render::SceneRenderer &renderer) noexcept;
  [[nodiscard]] bool
  Step(const Vec3 &eye, bool prepare, ResourcePublication publication, std::string &error);
  [[nodiscard]] bool Ready() const;
  [[nodiscard]] size_t Resident() const;
  [[nodiscard]] bool AwaitProgress(double seconds);
  [[nodiscard]] Counters Costs() const noexcept;

  [[nodiscard]] size_t Wanted() const { return Groups_.size(); }

private:
  enum class Phase { Wanted, Reading, Missing, Preparing, Prepared, Resident };

  struct Group {
    const Generators::TreeSpecies *Species = nullptr;
    std::string Provenance;
    std::vector<Mat4> Models;
    std::unique_ptr<Render::ImpostorInstances> Pieces;
    Phase State = Phase::Wanted;
  };

  VegetationStreaming(Render::SceneRenderer &renderer, Tasks &compute, const Config &config);
  bool PollPreparation(bool prepare, std::string &error);
  bool AcceptCacheResult(std::string &error);
  void PrepareNext();
  Render::SceneRenderer *Renderer_;
  std::vector<Group> Groups_;
  Tasks *Preparation_;
  Content::PreparedImpostorAssets Cache_;
  Content::ImpostorAtlasShape Shape_;
  std::optional<Content::ImpostorAtlas> PreparedAtlas_;
  Tasks::Handle Preparing_ = Tasks::kNoTask;
  size_t PreparingGroup_ = 0;
  std::string PreparedError_, Failure_;
  std::atomic_uint64_t Generated_{0};
  std::atomic<double> GenerationMs_{0};
};
}
#endif
