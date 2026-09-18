#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "Asset.h"
#include "Digest.h"

namespace outshine::Core {

constexpr uint64_t kDigestModulus = 1000000007ull;

void Posed::Clears() {
  Assets_.clear();
  Built_.clear();
  HoldsBuilt_ = false;
  Camera_.reset();
  Read_ = false;
  Moves_ = false;
  Frames_ = 1;
  AtS_ = 0.0;
  DurationS_ = 0.0;
  LocalsDigest_ = 0.0;
  AssembledDigest_ = 0.0;
  Changed_ += 1;
}

void Posed::Carries(outshine::Geometry &&built) {
  Clears();
  Built_ = std::move(built);
  Asset asset;
  asset.Snapshot = Built_.clone();
  Assets_.push_back(std::move(asset));
  HoldsBuilt_ = true;
}

bool Posed::Reads(const Sited &asset,
                  Scenario::AssetAnimation animation,
                  Playing at,
                  std::string &error) {
  if (Read_) { return true; }
  Posed importedAsset;
  Asset imported;
  imported.Importer = std::make_unique<GltfImporter>();
  if (auto loaded = imported.Importer->load(asset.Path); !loaded) {
    error = std::move(loaded.error());
    return false;
  }
  if (!asset.Variant.empty()) {
    if (auto selected = imported.Importer->selectMaterialVariant(asset.Variant); !selected) {
      error = std::move(selected.error());
      return false;
    }
  }
  const bool animates =
      animation == Scenario::AssetAnimation::Play || animation == Scenario::AssetAnimation::Loop;
  if (animates && imported.Importer->animationCount() > 0) {
    const std::array clips{at.Clip};
    if (auto selected = imported.Importer->selectAnimations(clips); !selected) {
      error = std::move(selected.error());
      return false;
    }
  }
  imported.Snapshot = imported.Importer->geometry().clone();
  importedAsset.Assets_.push_back(std::move(imported));
  importedAsset.HoldsBuilt_ = true;
  importedAsset.Read_ = true;
  importedAsset.DurationS_ = importedAsset.Assets_.front().Importer->durationS();
  importedAsset.Moves_ = importedAsset.DurationS_ > 0.0;
  importedAsset.Frames_ =
      importedAsset.Moves_
          ? std::max(1, static_cast<int>(std::lround(importedAsset.DurationS_ * at.Fps)))
          : 1;
  if (!importedAsset.Rebuild(
          std::span<const outshine::Geometry>(&importedAsset.Assets_.front().Snapshot, 1), error)) {
    return false;
  }
  importedAsset.RefreshCamera();
  if (Assets_.empty()) {
    importedAsset.Changed_ = Changed_ + 1;
    *this = std::move(importedAsset);
    return true;
  }
  if (!Appends(std::move(importedAsset), error)) { return false; }
  Read_ = true;
  RefreshCamera();
  return true;
}

bool Posed::Appends(Posed &&more, std::string &error) {
  std::vector<outshine::Geometry> snapshots;
  snapshots.reserve(Assets_.size() + more.Assets_.size());
  for (const Asset &asset : Assets_) { snapshots.push_back(asset.Snapshot.clone()); }
  for (const Asset &asset : more.Assets_) { snapshots.push_back(asset.Snapshot.clone()); }
  if (!Rebuild(snapshots, error)) { return false; }
  Assets_.insert(Assets_.end(),
                 std::make_move_iterator(more.Assets_.begin()),
                 std::make_move_iterator(more.Assets_.end()));
  DurationS_ = std::max(DurationS_, more.DurationS_);
  Moves_ = Moves_ || more.Moves_;
  Frames_ = std::max(Frames_, more.Frames_);
  Changed_ += 1;
  return true;
}

bool Posed::Measures(double seconds, std::string &error) {
  return PoseInto(seconds, error);
}

bool Posed::Poses(double seconds, std::string &error) {
  return PoseInto(seconds, error);
}

bool Posed::PoseInto(double seconds, std::string &error) {
  if (Assets_.empty()) { return true; }
  std::vector<outshine::Geometry> snapshots;
  snapshots.reserve(Assets_.size());
  for (Asset &asset : Assets_) {
    if (asset.Importer != nullptr) {
      if (auto sampled = asset.Importer->sampleAnimation(seconds); !sampled) {
        error = std::move(sampled.error());
        return false;
      }
      snapshots.push_back(asset.Importer->geometry().clone());
    } else {
      snapshots.push_back(asset.Snapshot.clone());
    }
  }
  if (!Rebuild(snapshots, error)) { return false; }
  for (size_t at = 0; at < Assets_.size(); ++at) {
    Assets_[at].Snapshot = std::move(snapshots[at]);
  }
  AtS_ = seconds;
  RefreshCamera();
  Changed_ += 1;
  return true;
}

bool Posed::Rebuild(std::span<const outshine::Geometry> snapshots, std::string &error) {
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
  Built_ = std::move(candidate);
  HoldsBuilt_ = true;
  RefreshDigests();
  return true;
}

void Posed::RefreshCamera() {
  Camera_.reset();
  for (const Asset &asset : Assets_) {
    if (asset.Importer == nullptr || asset.Importer->cameraCount() == 0) { continue; }
    const auto imported = asset.Importer->camera(0);
    if (!imported) { continue; }
    const auto viewpoint = Render::ViewpointOf(*imported);
    if (viewpoint) { Camera_ = *viewpoint; }
    return;
  }
}

void Posed::RefreshDigests() {
  uint64_t locals = kDigestBasis;
  uint64_t vertices = kDigestBasis;
  for (int part = 0; part < Built_.parts(); ++part) {
    for (const double value : Built_.placementOf(part)) {
      locals = (locals ^ std::bit_cast<uint64_t>(value)) * kDigestPrime;
    }
    for (const float value : Built_.positionsOf(part)) {
      vertices = (vertices ^ std::bit_cast<uint32_t>(value)) * kDigestPrime;
    }
  }
  LocalsDigest_ = static_cast<double>(locals % kDigestModulus);
  AssembledDigest_ = static_cast<double>(vertices % kDigestModulus);
}

}
