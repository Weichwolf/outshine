#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "AnimatedAsset.h"
#include "Digest.h"
#include "ScenePlayback.h"

namespace outshine {

constexpr uint64_t kDigestModulus = 1000000007ull;

ScenePlayback::ScenePlayback() = default;
ScenePlayback::~ScenePlayback() = default;
ScenePlayback::ScenePlayback(ScenePlayback &&) noexcept = default;
ScenePlayback &ScenePlayback::operator=(ScenePlayback &&) noexcept = default;

void ScenePlayback::Clear() {
  Assets_.clear();
  Geometry_.clear();
  HasGeometry_ = false;
  Camera_.reset();
  Loaded_ = false;
  Animated_ = false;
  FrameCount_ = 1;
  TimeS_ = 0.0;
  DurationS_ = 0.0;
  PlacementDigest_ = 0.0;
  VertexDigest_ = 0.0;
  Revision_ += 1;
}

void ScenePlayback::SetGeometry(outshine::Geometry &&geometry) {
  Clear();
  Geometry_ = std::move(geometry);
  Asset asset;
  asset.Snapshot = Geometry_.clone();
  Assets_.push_back(std::move(asset));
  HasGeometry_ = true;
}

bool ScenePlayback::Load(const AssetRequest &asset,
                         bool animate,
                         PlaybackSettings settings,
                         std::string &error) {
  if (Loaded_) { return true; }
  ScenePlayback importedAsset;
  Asset imported;
  imported.Animator = std::make_unique<AnimatedAsset>();
  if (auto loaded = imported.Animator->load(asset.Path); !loaded) {
    error = std::move(loaded.error());
    return false;
  }
  if (!asset.Variant.empty()) {
    if (auto selected = imported.Animator->selectMaterialVariant(asset.Variant); !selected) {
      error = std::move(selected.error());
      return false;
    }
  }
  if (animate && imported.Animator->animationCount() > 0) {
    const std::array clips{settings.Clip};
    if (auto selected = imported.Animator->selectAnimations(clips); !selected) {
      error = std::move(selected.error());
      return false;
    }
  }
  imported.Snapshot = imported.Animator->geometry().clone();
  if (imported.Animator->cameraCount() > 0) {
    const auto camera = imported.Animator->camera(0);
    if (camera) { imported.Camera = *camera; }
  }
  importedAsset.DurationS_ = imported.Animator->durationS();
  importedAsset.Animated_ = importedAsset.DurationS_ > 0.0;
  if (!importedAsset.Animated_) { imported.Animator.reset(); }
  importedAsset.Assets_.push_back(std::move(imported));
  importedAsset.HasGeometry_ = true;
  importedAsset.Loaded_ = true;
  importedAsset.FrameCount_ =
      importedAsset.Animated_
          ? std::max(1, static_cast<int>(std::lround(importedAsset.DurationS_ * settings.Fps)))
          : 1;
  if (!importedAsset.Rebuild(
          std::span<const outshine::Geometry>(&importedAsset.Assets_.front().Snapshot, 1), error)) {
    return false;
  }
  importedAsset.RefreshCamera();
  if (Assets_.empty()) {
    importedAsset.Revision_ = Revision_ + 1;
    *this = std::move(importedAsset);
    return true;
  }
  if (!Append(std::move(importedAsset), error)) { return false; }
  Loaded_ = true;
  RefreshCamera();
  return true;
}

bool ScenePlayback::Append(ScenePlayback &&more, std::string &error) {
  std::vector<outshine::Geometry> snapshots;
  snapshots.reserve(Assets_.size() + more.Assets_.size());
  for (const Asset &asset : Assets_) { snapshots.push_back(asset.Snapshot.clone()); }
  for (const Asset &asset : more.Assets_) { snapshots.push_back(asset.Snapshot.clone()); }
  if (!Rebuild(snapshots, error)) { return false; }
  Assets_.insert(Assets_.end(),
                 std::make_move_iterator(more.Assets_.begin()),
                 std::make_move_iterator(more.Assets_.end()));
  DurationS_ = std::max(DurationS_, more.DurationS_);
  Animated_ = Animated_ || more.Animated_;
  FrameCount_ = std::max(FrameCount_, more.FrameCount_);
  Revision_ += 1;
  return true;
}

bool ScenePlayback::Sample(double seconds, std::string &error) {
  return PoseInto(seconds, error);
}

bool ScenePlayback::PoseInto(double seconds, std::string &error) {
  if (Assets_.empty()) { return true; }
  if (!Animated_) {
    TimeS_ = seconds;
    return true;
  }
  std::vector<outshine::Geometry> snapshots;
  std::vector<std::optional<outshine::Camera>> cameras;
  snapshots.reserve(Assets_.size());
  cameras.reserve(Assets_.size());
  for (Asset &asset : Assets_) {
    if (asset.Animator != nullptr) {
      if (auto sampled = asset.Animator->sampleAnimation(seconds); !sampled) {
        error = std::move(sampled.error());
        return false;
      }
      snapshots.push_back(asset.Animator->geometry().clone());
      std::optional<outshine::Camera> camera;
      if (asset.Animator->cameraCount() > 0) {
        const auto sampled = asset.Animator->camera(0);
        if (sampled) { camera = *sampled; }
      }
      cameras.push_back(camera);
    } else {
      snapshots.push_back(asset.Snapshot.clone());
      cameras.push_back(asset.Camera);
    }
  }
  if (!Rebuild(snapshots, error)) { return false; }
  for (size_t at = 0; at < Assets_.size(); ++at) {
    Assets_[at].Snapshot = std::move(snapshots[at]);
    Assets_[at].Camera = cameras[at];
  }
  TimeS_ = seconds;
  RefreshCamera();
  Revision_ += 1;
  return true;
}

bool ScenePlayback::Rebuild(std::span<const outshine::Geometry> snapshots, std::string &error) {
  outshine::Geometry candidate;
  bool holds = false;
  for (const outshine::Geometry &snapshot : snapshots) {
    if (snapshot.parts() == 0) { continue; }
    if (!holds) {
      candidate = snapshot.clone();
      holds = true;
      continue;
    }
    if (const auto appended = candidate.append(snapshot); !appended) {
      error = std::string("native imported assets could not be joined: ") +
              (appended.error() == GeometryAppendError::MalformedSource ? "malformed source"
                                                                        : "capacity exceeded");
      return false;
    }
  }
  Geometry_ = std::move(candidate);
  HasGeometry_ = true;
  RefreshDigests();
  return true;
}

void ScenePlayback::RefreshCamera() {
  Camera_.reset();
  for (const Asset &asset : Assets_) {
    if (!asset.Camera) { continue; }
    Camera_ = *asset.Camera;
    return;
  }
}

void ScenePlayback::RefreshDigests() {
  uint64_t locals = kDigestBasis;
  uint64_t vertices = kDigestBasis;
  for (int part = 0; part < Geometry_.parts(); ++part) {
    for (const double value : Geometry_.placementOf(part)) {
      locals = (locals ^ std::bit_cast<uint64_t>(value)) * kDigestPrime;
    }
    for (const float value : Geometry_.positionsOf(part)) {
      vertices = (vertices ^ std::bit_cast<uint32_t>(value)) * kDigestPrime;
    }
  }
  PlacementDigest_ = static_cast<double>(locals % kDigestModulus);
  VertexDigest_ = static_cast<double>(vertices % kDigestModulus);
}

}
