#ifndef OUTSHINE_ENGINE_STREAMING_VEGETATIONSTREAMING_H
#define OUTSHINE_ENGINE_STREAMING_VEGETATIONSTREAMING_H

#include "ImpostorCache.h"
#include "ImpostorAtlasShape.h"
#include "ImpostorInstances.h"
#include "WorldPlacement.h"
#include "Shipped.h"

namespace outshine {
class VegetationStreaming {
public:
  struct Config {
    Data::ImpostorCache::Config Cache;
    Content::ImpostorAtlasShape Shape;
    size_t Prototypes = 64;
    size_t Instances = 65536;
  };

  static std::unique_ptr<VegetationStreaming> Create(Render::SceneRenderer &renderer,
                                                     const Generators::Shipping &catalogue,
                                                     std::span<const WorldInstance> instances,
                                                     const TangentFrame &frame,
                                                     const Config &config,
                                                     std::string &error);
  ~VegetationStreaming();
  void Into(Render::SceneRenderer &renderer) noexcept;
  [[nodiscard]] bool Step(const Vec3 &eye, bool prepare, std::string &error);
  [[nodiscard]] bool Ready() const;
  [[nodiscard]] size_t Resident() const;

  [[nodiscard]] size_t Wanted() const { return Groups_.size(); }

private:
  enum class Phase { Wanted, Reading, Missing, Preparing, Resident };

  struct Group {
    const Generators::TreeSpecies *Species = nullptr;
    std::string Provenance;
    std::vector<Mat4> Models;
    std::unique_ptr<Render::ImpostorInstances> Pieces;
    Phase State = Phase::Wanted;
  };

  VegetationStreaming(Render::SceneRenderer &renderer, const Config &config);
  bool PollPreparation(bool prepare, std::string &error);
  bool AcceptCacheResult(std::string &error);
  void PrepareNext();
  Render::SceneRenderer *Renderer_;
  std::vector<Group> Groups_;
  Tasks Io_{2};
  Tasks Preparation_{1};
  Data::ImpostorCache Cache_;
  Content::ImpostorAtlasShape Shape_;
  Tasks::Handle Preparing_ = Tasks::kNoTask;
  size_t PreparingGroup_ = 0;
  std::string PreparedError_, Failure_;
};
}
#endif
