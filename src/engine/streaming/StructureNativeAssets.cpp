#include "StructureBuildQueue.h"
#include "PreparedBuildingAssets.h"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
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
  if (selected.RequestKey.empty()) {
    NativeFailure_ = Generators::StructureBakeErrorKind::ArtifactInvalidProduct;
    return false;
  }
  const auto found = NativeLookups_.find(selected.RequestKey);
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
    output->Task = Pool_->Post([output, cache, request = selected.RequestKey] {
      const auto began = std::chrono::steady_clock::now();
      output->Result = cache->LoadBasisRequest(request);
      output->ReadMs =
          std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began)
              .count();
    });
    NativeLookups_.emplace(selected.RequestKey, std::move(lookup));
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
  selected.ReadMs = lookup.ReadMs;
  selected.StreetDigest = lookup.StreetDigest;
  if (*lookup.Result) {
    selected.Key = (**lookup.Result).BaseKey;
    selected.Basis = (**lookup.Result).Product;
    return true;
  }
  if (lookup.Stage == NativeLookup::Phase::Content) {
    selected.Key = lookup.ContentKey;
    return true;
  }
  const auto digest = stack.Ways().SourceDigest(vectors, tile);
  if (!stack.Ways().Ingested(vectors) || !digest) {
    stack.RequestStreets();
    return false;
  }
  const auto key =
      cache->Key(address, *digest, stack.Pool().Shaped(), prints.TileSpanM(), region.InputDigest);
  if (key.empty()) {
    NativeFailure_ = Generators::StructureBakeErrorKind::ArtifactInvalidProduct;
    return false;
  }
  lookup.Stage = NativeLookup::Phase::Content;
  lookup.ContentKey = key;
  lookup.StreetDigest = *digest;
  lookup.Task = Pool_->Post([output = &lookup, cache, key, request = selected.RequestKey] {
    const auto began = std::chrono::steady_clock::now();
    auto basis = cache->LoadBasis(key, request);
    if (!basis) {
      output->Result = std::unexpected(basis.error());
    } else if (*basis) {
      output->Result = Generators::PreparedBuildingAssets::RequestedBasis{
          .BaseKey = key, .Product = std::move(*basis)};
    } else {
      output->Result = std::optional<Generators::PreparedBuildingAssets::RequestedBasis>{};
    }
    output->ReadMs +=
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  });
  return false;
}
}
