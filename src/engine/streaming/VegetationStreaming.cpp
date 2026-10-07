#include "VegetationStreaming.h"
#include "ImpostorPreparation.h"
#include "TreePrototype.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>
#include "SceneRenderer.h"
#include <algorithm>
#include <chrono>
#include <numeric>
#include <ratio>

namespace outshine {
namespace Says {
constexpr auto Instances = "vegetation streaming instance capacity exceeded";
constexpr auto Prototypes = "vegetation streaming prototype capacity exceeded";
constexpr auto Preparation = "vegetation preparation must finish before realtime updates";
constexpr auto Tree = "vegetation source cannot produce its tree prototype";
}

VegetationStreaming::VegetationStreaming(Render::SceneRenderer &renderer,
                                         Tasks &compute,
                                         const Config &config)
    : Renderer_(&renderer),
      Preparation_(&compute),
      Cache_(compute, config.Cache),
      Shape_(config.Shape) {}

VegetationStreaming::~VegetationStreaming() {
  if (Preparing_ != Tasks::kNoTask) { Preparation_->Wait(Preparing_); }
}

std::unique_ptr<VegetationStreaming>
VegetationStreaming::Create(Render::SceneRenderer &renderer,
                            Tasks &compute,
                            const Generators::Shipping &catalogue,
                            std::span<const WorldInstance> instances,
                            const TangentFrame &frame,
                            const Config &config,
                            std::string &error) {
  if (instances.size() > config.Instances) {
    error = Says::Instances;
    return nullptr;
  }
  auto result =
      std::unique_ptr<VegetationStreaming>(new VegetationStreaming(renderer, compute, config));
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
    if (Preparation_->TakeCompletion(Preparing_)) {
      Preparing_ = Tasks::kNoTask;
      if (!PreparedError_.empty()) {
        Failure_ = PreparedError_;
        error = Failure_;
        return false;
      }
      Groups_[PreparingGroup_].State = PreparedAtlas_ ? Phase::Prepared : Phase::Wanted;
    } else if (!prepare) {
      error = Says::Preparation;
      return false;
    }
  }
  return true;
}

bool VegetationStreaming::AcceptCacheResult(std::string &error) {
  const bool prepared = Preparing_ == Tasks::kNoTask && PreparedAtlas_.has_value();
  auto loaded = prepared ? std::optional<Content::PreparedImpostorAssets::Loaded>(
                               {.Provenance = Groups_[PreparingGroup_].Provenance,
                                .Atlas = std::move(PreparedAtlas_),
                                .Error = {}})
                         : Cache_.Take();
  if (prepared) { PreparedAtlas_.reset(); }
  if (!loaded) { return true; }
  for (auto &group : Groups_) {
    if ((group.State != Phase::Reading && group.State != Phase::Prepared &&
         group.State != Phase::Missing) ||
        group.Provenance != loaded->Provenance) {
      continue;
    }
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
  if (Preparing_ == Tasks::kNoTask && !PreparedAtlas_) {
    const auto missing = std::ranges::find_if(Groups_, [&](const Group &candidate) {
      return candidate.State == Phase::Missing &&
             std::ranges::none_of(Groups_, [&](const Group &group) {
               return group.Provenance == candidate.Provenance &&
                      (group.State == Phase::Wanted || group.State == Phase::Reading ||
                       group.State == Phase::Preparing || group.State == Phase::Prepared);
             });
    });
    if (missing != Groups_.end()) {
      PreparingGroup_ = static_cast<size_t>(missing - Groups_.begin());
      missing->State = Phase::Preparing;
      PreparedError_.clear();
      Preparing_ = Preparation_->Post([this] {
        const auto began = std::chrono::steady_clock::now();
        const auto &group = Groups_[PreparingGroup_];
        const auto tree = Generators::TreePrototype::Grow(*group.Species);
        if (!tree) {
          PreparedError_ = Says::Tree;
          return;
        }
        PreparedAtlas_ = BakeImpostorAtlas(*tree, Shape_, PreparedError_);
        ++Generated_;
        GenerationMs_ +=
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began)
                .count();
        if (PreparedAtlas_ && Cache_.Enabled()) {
          (void)Cache_.Publish(*PreparedAtlas_, group.Provenance, PreparedError_);
          PreparedAtlas_.reset();
        }
      });
    }
  }
}

bool VegetationStreaming::Step(const Vec3 &eye,
                               bool prepare,
                               ResourcePublication publication,
                               std::string &error) {
  if (!Failure_.empty()) {
    error = Failure_;
    return false;
  }
  if (!PollPreparation(prepare, error)) { return false; }
  if (publication == ResourcePublication::Allowed && !AcceptCacheResult(error)) { return false; }
  for (auto &group : Groups_) {
    if (group.State != Phase::Wanted) { continue; }
    if (!Cache_.Enabled()) {
      group.State = Phase::Missing;
      continue;
    }
    if (Cache_.Read(group.Provenance) == Content::PreparedImpostorAssets::Request::Full) { break; }
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

bool VegetationStreaming::AwaitProgress(double seconds) {
  return Preparing_ != Tasks::kNoTask ? Preparation_->AwaitCompletion(Preparing_, seconds)
                                      : Cache_.AwaitProgress(seconds);
}

VegetationStreaming::Counters VegetationStreaming::Costs() const noexcept {
  return {.Assets = Cache_.Costs(), .Generated = Generated_, .GenerationMs = GenerationMs_};
}
}
