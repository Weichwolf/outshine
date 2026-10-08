#include "StructureBuildQueue.h"
#include "PreparedBuildingAssets.h"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <ratio>
#include <utility>

namespace outshine {
size_t StructureBuildQueue::PendingBasisLookups() const noexcept {
  return static_cast<size_t>(std::ranges::count_if(
      NativeLookups_, [](const auto &entry) { return entry.second->Task != Tasks::kNoTask; }));
}

bool StructureBuildQueue::SelectNativeBasis(
    Ground::SurfacePreparation &stack,
    const ::outshine::Generators::Osm::BuildingField &prints,
    uint32_t tile,
    VectorSelection &selected) {
  const auto &cache = stack.BuildingAssets();
  if (!cache) { return true; }
  const auto &vectors = *stack.Vectors();
  const auto &region = vectors.Tiles()[tile];
  if (region.InputDigest.empty()) { return true; }
  const Data::TileId address{
      .Zoom = region.Z, .X = static_cast<uint32_t>(region.X), .Y = static_cast<uint32_t>(region.Y)};
  selected.RequestKey = cache->RequestKey(address, stack.Pool().Shaped(), prints.TileSpanM());
  const auto digest = stack.Ways().SourceDigest(vectors, tile);
  if (!digest) { return false; }
  selected.StreetDigest = *digest;
  selected.Key =
      cache->Key(address, *digest, stack.Pool().Shaped(), prints.TileSpanM(), region.InputDigest);
  if (selected.Key.empty() || selected.RequestKey.empty()) {
    NativeFailure_ = Generators::StructureBakeErrorKind::ArtifactInvalidProduct;
    return false;
  }
  const auto found = NativeLookups_.find(selected.Key);
  if (found == NativeLookups_.end()) {
    if (PendingBasisLookups() >= kCandidateWindow) { return false; }
    constexpr size_t kLookupsMost = 256;
    if (NativeLookups_.size() >= kLookupsMost) {
      const auto idle = std::ranges::find_if(
          NativeLookups_, [](const auto &entry) { return entry.second->Task == Tasks::kNoTask; });
      if (idle == NativeLookups_.end()) { return false; }
      NativeLookups_.erase(idle);
    }
    auto lookup = std::make_unique<NativeLookup>();
    auto *const output = lookup.get();
    output->Task = Pool_->Post([output, cache, key = selected.Key, request = selected.RequestKey] {
      const auto began = std::chrono::steady_clock::now();
      auto requested = cache->LoadBasisRequest(request);
      if (!requested) {
        output->Result = std::unexpected(requested.error());
      } else if (*requested && (**requested).BaseKey == key) {
        output->Result = std::move((**requested).Product);
      } else {
        output->Result = cache->LoadBasis(key, request);
      }
      output->ReadMs =
          std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began)
              .count();
    });
    NativeLookups_.emplace(selected.Key, std::move(lookup));
    return false;
  }
  auto &lookup = *found->second;
  if (lookup.Task != Tasks::kNoTask) {
    if (!Pool_->TakeCompletion(lookup.Task)) { return false; }
    lookup.Task = Tasks::kNoTask;
  }
  if (!lookup.Result) {
    NativeFailure_ = lookup.Result.error();
    return false;
  }
  selected.Basis = *lookup.Result;
  selected.ReadMs = lookup.ReadMs;
  return true;
}
}
