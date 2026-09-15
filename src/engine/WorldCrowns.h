#ifndef OUTSHINE_ENGINE_WORLDCROWNS_H
#define OUTSHINE_ENGINE_WORLDCROWNS_H

#include "CrownCache.h"
#include "CrownPieces.h"
#include "WorldPlacement.h"
#include "Shipped.h"

namespace outshine {
class WorldCrowns {
public:
  struct Config {
    CrownCache::Config Cache;
    CrownAtlas::Shape Shape;
    size_t Prototypes = 64;
    size_t Instances = 65536;
  };

  static std::unique_ptr<WorldCrowns> Create(Core::Live &live,
                                             const Generators::Shipping &catalogue,
                                             std::span<const WorldInstance> instances,
                                             const TangentFrame &frame,
                                             const Config &config,
                                             std::string &error);
  ~WorldCrowns();
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
    std::unique_ptr<CrownPieces> Pieces;
    Phase State = Phase::Wanted;
  };

  WorldCrowns(Core::Live &live, const Config &config);
  bool PollPreparation(bool prepare, std::string &error);
  bool AcceptCacheResult(std::string &error);
  void PrepareNext();
  Core::Live *Live_;
  std::vector<Group> Groups_;
  Tasks Io_{2};
  Tasks Preparation_{1};
  CrownCache Cache_;
  CrownAtlas::Shape Shape_;
  Tasks::Handle Preparing_ = Tasks::kNoTask;
  size_t PreparingGroup_ = 0;
  std::string PreparedError_, Failure_;
};
}
#endif
