#include "WorldCrowns.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>
#include "Live.h"
#include <algorithm>
#include <numeric>

namespace outshine {
namespace Says {
constexpr auto Instances = "world crown instance capacity exceeded";
constexpr auto Prototypes = "world crown prototype capacity exceeded";
constexpr auto Preparation = "world crown preparation must finish before realtime updates";
constexpr auto Tree = "world crown source cannot produce its tree prototype";
}

WorldCrowns::WorldCrowns(Core::Live &live, const Config &config)
    : Live_(&live), Cache_(Io_, config.Cache), Shape_(config.Shape) {}

WorldCrowns::~WorldCrowns() {
  if (Preparing_ != Tasks::kNoTask) { Preparation_.Wait(Preparing_); }
}

std::unique_ptr<WorldCrowns> WorldCrowns::Create(Core::Live &live,
                                                 const Generators::Shipping &catalogue,
                                                 std::span<const WorldInstance> instances,
                                                 const TangentFrame &frame,
                                                 const Config &config,
                                                 std::string &error) {
  if (instances.size() > config.Instances) {
    error = Says::Instances;
    return nullptr;
  }
  auto result = std::unique_ptr<WorldCrowns>(new WorldCrowns(live, config));
  std::vector<size_t> order(instances.size());
  std::ranges::iota(order, size_t{0});
  std::ranges::sort(
      order, [&](size_t a, size_t b) { return instances[a].Cluster < instances[b].Cluster; });
  uint32_t previous = 0;
  for (const size_t at : order) {
    const auto &instance = instances[at];
    const auto *species = catalogue.TreeFor(Generators::ClusterId{instance.Cluster});
    if (species == nullptr) { continue; }
    if (result->Groups_.empty() || previous != instance.Cluster) {
      if (result->Groups_.size() >= config.Prototypes) {
        error = Says::Prototypes;
        return nullptr;
      }
      Group group;
      group.Species = species;
      group.Provenance = CrownAtlas::ProvenanceFor(species->Definition(), config.Shape);
      result->Groups_.push_back(std::move(group));
      previous = instance.Cluster;
    }
    result->Groups_.back().Models.push_back(instance.Where.ModelIn(frame));
  }
  return result;
}

bool WorldCrowns::Ready() const {
  return std::ranges::all_of(Groups_,
                             [](const Group &group) { return group.State == Phase::Resident; });
}

size_t WorldCrowns::Resident() const {
  return static_cast<size_t>(std::ranges::count_if(
      Groups_, [](const Group &group) { return group.State == Phase::Resident; }));
}

bool WorldCrowns::PollPreparation(bool prepare, std::string &error) {
  if (Preparing_ != Tasks::kNoTask) {
    if (Preparation_.Done(Preparing_)) {
      Preparing_ = Tasks::kNoTask;
      if (!PreparedError_.empty()) {
        Failure_ = PreparedError_;
        error = Failure_;
        return false;
      }
      Groups_[PreparingGroup_].State = Phase::Wanted;
    } else if (!prepare) {
      error = Says::Preparation;
      return false;
    }
  }
  return true;
}

bool WorldCrowns::AcceptCacheResult(std::string &error) {
  auto loaded = Cache_.Take();
  if (!loaded) { return true; }
  for (auto &group : Groups_) {
    if (group.State != Phase::Reading || group.Provenance != loaded->Provenance) { continue; }
    if (!loaded->Atlas) {
      group.State = Phase::Missing;
      continue;
    }
    group.Pieces = CrownPieces::Create(
        *Live_, *loaded->Atlas, static_cast<uint32_t>(group.Models.size()), error);
    if (!group.Pieces) {
      Failure_ = error;
      return false;
    }
    group.State = Phase::Resident;
  }
  return true;
}

void WorldCrowns::PrepareNext() {
  if (Preparing_ == Tasks::kNoTask) {
    const auto missing = std::ranges::find(Groups_, Phase::Missing, &Group::State);
    if (missing != Groups_.end()) {
      PreparingGroup_ = static_cast<size_t>(missing - Groups_.begin());
      missing->State = Phase::Preparing;
      PreparedError_.clear();
      Preparing_ = Preparation_.Post([this] {
        const auto &group = Groups_[PreparingGroup_];
        const auto tree = Generators::TreePrototype::Grow(*group.Species);
        if (!tree) {
          PreparedError_ = Says::Tree;
          return;
        }
        auto atlas = CrownAtlas::Bake(*tree, Shape_, PreparedError_);
        if (atlas) { (void)Cache_.Publish(*atlas, group.Provenance, PreparedError_); }
      });
    }
  }
}

bool WorldCrowns::Step(const Vec3 &eye, bool prepare, std::string &error) {
  if (!Failure_.empty()) {
    error = Failure_;
    return false;
  }
  if (!PollPreparation(prepare, error) || !AcceptCacheResult(error)) { return false; }
  for (auto &group : Groups_) {
    if (group.State != Phase::Wanted) { continue; }
    if (Cache_.Read(group.Provenance) == CrownCache::Request::Full) { break; }
    group.State = Phase::Reading;
  }
  if (prepare) { PrepareNext(); }
  for (auto &group : Groups_) {
    if (group.Pieces && !group.Pieces->Update(group.Models, eye, error)) {
      Failure_ = error;
      return false;
    }
  }
  return true;
}
}
