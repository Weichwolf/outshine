#include "VegetationStreaming.h"
#include "ImpostorPreparation.h"
#include "TreePrototype.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>
#include "SceneRenderer.h"
#include <algorithm>
#include <numeric>

namespace outshine {
namespace Says {
constexpr auto Instances = "vegetation streaming instance capacity exceeded";
constexpr auto Prototypes = "vegetation streaming prototype capacity exceeded";
constexpr auto Preparation = "vegetation preparation must finish before realtime updates";
constexpr auto Tree = "vegetation source cannot produce its tree prototype";
}

VegetationStreaming::VegetationStreaming(Render::SceneRenderer &renderer, const Config &config)
    : Renderer_(&renderer), Cache_(Io_, config.Cache), Shape_(config.Shape) {}

VegetationStreaming::~VegetationStreaming() {
  if (Preparing_ != Tasks::kNoTask) { Preparation_.Wait(Preparing_); }
}

std::unique_ptr<VegetationStreaming>
VegetationStreaming::Create(Render::SceneRenderer &renderer,
                            const Generators::Shipping &catalogue,
                            std::span<const WorldInstance> instances,
                            const TangentFrame &frame,
                            const Config &config,
                            std::string &error) {
  if (instances.size() > config.Instances) {
    error = Says::Instances;
    return nullptr;
  }
  auto result = std::unique_ptr<VegetationStreaming>(new VegetationStreaming(renderer, config));
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
      group.Provenance = ImpostorAtlasProvenance(species->Definition(), config.Shape);
      result->Groups_.push_back(std::move(group));
      previous = instance.Cluster;
    }
    result->Groups_.back().Models.push_back(instance.Where.ModelIn(frame));
  }
  return result;
}

bool VegetationStreaming::Ready() const {
  return std::ranges::all_of(Groups_,
                             [](const Group &group) { return group.State == Phase::Resident; });
}

void VegetationStreaming::Into(Render::SceneRenderer &renderer) noexcept {
  Renderer_ = &renderer;
  for (Group &group : Groups_) {
    if (group.Pieces) { group.Pieces->MoveTo(renderer); }
  }
}

size_t VegetationStreaming::Resident() const {
  return static_cast<size_t>(std::ranges::count_if(
      Groups_, [](const Group &group) { return group.State == Phase::Resident; }));
}

bool VegetationStreaming::PollPreparation(bool prepare, std::string &error) {
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

bool VegetationStreaming::AcceptCacheResult(std::string &error) {
  auto loaded = Cache_.Take();
  if (!loaded) { return true; }
  for (auto &group : Groups_) {
    if (group.State != Phase::Reading || group.Provenance != loaded->Provenance) { continue; }
    if (!loaded->Atlas) {
      group.State = Phase::Missing;
      continue;
    }
    group.Pieces = Render::ImpostorInstances::Create(
        *Renderer_, *loaded->Atlas, static_cast<uint32_t>(group.Models.size()), error);
    if (!group.Pieces) {
      Failure_ = error;
      return false;
    }
    group.State = Phase::Resident;
  }
  return true;
}

void VegetationStreaming::PrepareNext() {
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
        auto atlas = BakeImpostorAtlas(*tree, Shape_, PreparedError_);
        if (atlas) { (void)Cache_.Publish(*atlas, group.Provenance, PreparedError_); }
      });
    }
  }
}

bool VegetationStreaming::Step(const Vec3 &eye, bool prepare, std::string &error) {
  if (!Failure_.empty()) {
    error = Failure_;
    return false;
  }
  if (!PollPreparation(prepare, error) || !AcceptCacheResult(error)) { return false; }
  for (auto &group : Groups_) {
    if (group.State != Phase::Wanted) { continue; }
    if (Cache_.Read(group.Provenance) == Data::ImpostorCache::Request::Full) { break; }
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
