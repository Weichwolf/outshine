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
#include <string>
#include <utility>

namespace outshine {
size_t StructureBuildQueue::PendingBasisLookups() const noexcept {
  return static_cast<size_t>(std::ranges::count_if(
      NativeLookups_, [](const auto &entry) { return entry.second->Task != Tasks::kNoTask; }));
}

StructureBuildQueue::NativeLookup *StructureBuildQueue::AdvanceNativeLookup(
    const std::shared_ptr<Generators::PreparedBuildingAssets> &cache, const std::string &request) {
  auto found = NativeLookups_.find(request);
  if (found != NativeLookups_.end()) {
    const auto &previous = *found->second;
    if (previous.Task == Tasks::kNoTask && previous.Stage == NativeLookup::Phase::Request &&
        previous.Result && !*previous.Result &&
        previous.BasisWrites != cache->Costs().BasisWrites) {
      NativeLookups_.erase(found);
      found = NativeLookups_.end();
    }
  }
  if (found == NativeLookups_.end()) {
    if (PendingBasisLookups() >= kCandidateWindow) { return nullptr; }
    constexpr size_t kLookupsMost = 256;
    if (NativeLookups_.size() >= kLookupsMost) {
      const auto idle = std::ranges::find_if(
          NativeLookups_, [](const auto &entry) { return entry.second->Task == Tasks::kNoTask; });
      if (idle == NativeLookups_.end()) { return nullptr; }
      NativeLookups_.erase(idle);
    }
    auto lookup = std::make_unique<NativeLookup>();
    lookup->BasisWrites = cache->Costs().BasisWrites;
    auto *const output = lookup.get();
    output->Task = Pool_->Post([output, cache, request] {
      const auto began = std::chrono::steady_clock::now();
      output->Result = cache->LoadBasisRequest(request);
      output->ReadMs =
          std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began)
              .count();
    });
    if (output->Task == Tasks::kNoTask) {
      output->Result = std::unexpected(Generators::StructureBakeErrorKind::Cancelled);
    }
    NativeLookups_.emplace(request, std::move(lookup));
    return output->Task == Tasks::kNoTask ? output : nullptr;
  }
  auto &lookup = *found->second;
  if (lookup.Task != Tasks::kNoTask) {
    if (!Pool_->TakeCompletion(lookup.Task)) { return nullptr; }
    lookup.Task = Tasks::kNoTask;
  }
  return &lookup;
}

std::expected<StructureBuildQueue::NativeBasisQuery, Generators::StructureBakeError>
StructureBuildQueue::PollNativeBasis(Ground::SurfacePreparation &stack,
                                     const Generators::Osm::BuildingField &prints,
                                     uint32_t tile) {
  NativeBasisQuery query;
  const auto &cache = stack.BuildingAssets();
  const auto *vectors = stack.Vectors();
  if (!cache || Pool_ == nullptr || vectors == nullptr || tile >= vectors->Tiles().size()) {
    return query;
  }
  const auto &region = vectors->Tiles()[tile];
  if (region.InputDigest.empty()) { return query; }
  if (NativeAssets_ && NativeAssets_ != cache) {
    return std::unexpected(Generators::StructureBakeErrorKind::ArtifactInvalidProduct);
  }
  NativeAssets_ = cache;
  const Data::TileId address{
      .Zoom = region.Z, .X = static_cast<uint32_t>(region.X), .Y = static_cast<uint32_t>(region.Y)};
  query.RequestKey =
      cache->RequestKey(address, stack.Pool().Shaped(), prints.TileSpanM(), region.InputDigest);
  if (query.RequestKey.empty()) {
    return std::unexpected(Generators::StructureBakeErrorKind::ArtifactInvalidProduct);
  }
  auto *lookup = AdvanceNativeLookup(cache, query.RequestKey);
  if (lookup == nullptr) {
    query.Where = NativeBasisQuery::Phase::Pending;
    return query;
  }
  if (!lookup->Result) { return std::unexpected(lookup->Result.error()); }
  if (*lookup->Result) {
    query.Where = NativeBasisQuery::Phase::Ready;
    query.Source = &**lookup->Result;
  } else {
    query.Where = NativeBasisQuery::Phase::Missing;
  }
  return query;
}

void StructureBuildQueue::ReleaseNativeBasis(const std::string &request) {
  const auto found = NativeLookups_.find(request);
  if (found == NativeLookups_.end()) { return; }
  const auto &lookup = *found->second;
  if (lookup.Task == Tasks::kNoTask && lookup.Stage == NativeLookup::Phase::Request &&
      lookup.Result && *lookup.Result) {
    NativeLookups_.erase(found);
  }
}

bool StructureBuildQueue::SelectNativeBasis(
    Ground::SurfacePreparation &stack,
    const ::outshine::Generators::Osm::BuildingField &prints,
    uint32_t tile,
    VectorSelection &selected) {
  const auto query = PollNativeBasis(stack, prints, tile);
  if (!query) {
    NativeFailure_ = query.error();
    return false;
  }
  if (query->Where == NativeBasisQuery::Phase::Unavailable) { return true; }
  selected.RequestKey = query->RequestKey;
  if (query->Where == NativeBasisQuery::Phase::Pending) { return false; }
  auto &lookup = *NativeLookups_.at(selected.RequestKey);
  const auto &cache = stack.BuildingAssets();
  const auto &vectors = *stack.Vectors();
  const auto &region = vectors.Tiles()[tile];
  const Data::TileId address{
      .Zoom = region.Z, .X = static_cast<uint32_t>(region.X), .Y = static_cast<uint32_t>(region.Y)};
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
          .BaseKey = key, .Product = std::move(*basis), .Bounds = {}};
    } else {
      output->Result = std::optional<Generators::PreparedBuildingAssets::RequestedBasis>{};
    }
    output->ReadMs +=
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  });
  return false;
}
}
