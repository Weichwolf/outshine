#include "StructureBuildQueue.h"
#include "PreparedBuildingAssets.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <ratio>
#include <string>
#include <utility>
#include <variant>

namespace outshine {
size_t StructureBuildQueue::PendingBasisLookups() const noexcept {
  return static_cast<size_t>(std::ranges::count_if(NativeLookups_, [](const auto &entry) {
    return entry.second->Task != Tasks::kNoTask ||
           entry.second->Stage == NativeLookup::Phase::Bind || entry.second->Read.Pending();
  }));
}

void StructureBuildQueue::ReadNativeLookup(Generators::PreparedBuildingAssets &cache,
                                           const std::string &request,
                                           NativeLookup &lookup) {
  const auto cancel = [&lookup] {
    lookup.Read = Generators::PreparedBuildingAssets::BasisRead{};
    lookup.HeldBasis.reset();
    lookup.Result = std::unexpected(Generators::StructureBakeErrorKind::Cancelled);
  };
  if (lookup.Stopping.load(std::memory_order_relaxed)) {
    cancel();
    return;
  }
  if (lookup.Stage == NativeLookup::Phase::Request) {
    lookup.Result = cache.ReadBasisRequest(request, lookup.Read, lookup.Stopping);
    if (lookup.Result && !lookup.Read.Pending()) { lookup.HeldBasis.reset(); }
    return;
  }
  if (!lookup.HeldBasis) {
    auto basis = cache.ReadBasis(lookup.ContentKey, lookup.Read, lookup.Stopping);
    if (!basis) {
      lookup.Result = std::unexpected(basis.error());
      return;
    }
    if (!*basis) {
      lookup.Result = std::optional<Generators::PreparedBuildingAssets::RequestedBasis>{};
      return;
    }
    lookup.HeldBasis = std::move(*basis);
  }
  if (lookup.Stopping.load(std::memory_order_relaxed)) {
    cancel();
    return;
  }
  lookup.Stage = NativeLookup::Phase::Bind;
  const auto bound = cache.BindBasisRequestIfUnbound(lookup.ContentKey, request);
  if (!bound) {
    lookup.Result = std::unexpected(bound.error());
    return;
  }
  lookup.Read = Generators::PreparedBuildingAssets::BasisRead{};
  lookup.Stage = NativeLookup::Phase::Request;
  lookup.Result = cache.ReadBasisRequest(request, lookup.Read, lookup.Stopping);
  if (lookup.Result && !lookup.Read.Pending()) { lookup.HeldBasis.reset(); }
}

void StructureBuildQueue::StartNativeLookup(
    const std::shared_ptr<Generators::PreparedBuildingAssets> &cache,
    const std::string &request,
    NativeLookup &lookup) {
  lookup.Task = Pool_->Post([output = &lookup, cache, request] {
    const auto began = std::chrono::steady_clock::now();
    ReadNativeLookup(*cache, request, *output);
    const auto *kind =
        output->Result ? nullptr
                       : std::get_if<Generators::StructureBakeErrorKind>(&output->Result.error());
    if (kind != nullptr && *kind == Generators::StructureBakeErrorKind::ArtifactBusy) {
      const auto exponent = std::min(output->Retries, 4U);
      output->Retries = std::min(output->Retries + 1U, 4U);
      output->RetryAt =
          std::chrono::steady_clock::now() + std::chrono::milliseconds(int64_t{1} << exponent);
    } else {
      output->Retries = 0;
    }
    output->ReadMs +=
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  });
  if (lookup.Task == Tasks::kNoTask) {
    lookup.Result = std::unexpected(Generators::StructureBakeErrorKind::Cancelled);
  }
}

StructureBuildQueue::NativeLookup *StructureBuildQueue::RetryNativeLookup(
    const std::shared_ptr<Generators::PreparedBuildingAssets> &cache,
    const std::string &request,
    NativeLookup &lookup) {
  if (std::chrono::steady_clock::now() < lookup.RetryAt) { return nullptr; }
  const auto running = static_cast<size_t>(std::ranges::count_if(
      NativeLookups_, [](const auto &entry) { return entry.second->Task != Tasks::kNoTask; }));
  if (running >= kCandidateWindow ||
      (lookup.Stage != NativeLookup::Phase::Bind && !lookup.Read.Pending() &&
       PendingBasisLookups() >= kCandidateWindow)) {
    return nullptr;
  }
  StartNativeLookup(cache, request, lookup);
  return lookup.Task == Tasks::kNoTask ? &lookup : nullptr;
}

StructureBuildQueue::NativeLookup *StructureBuildQueue::AdvanceNativeLookup(
    const std::shared_ptr<Generators::PreparedBuildingAssets> &cache, const std::string &request) {
  auto found = NativeLookups_.find(request);
  if (found != NativeLookups_.end()) {
    const auto &previous = *found->second;
    if (previous.Task == Tasks::kNoTask && previous.Stage == NativeLookup::Phase::Request &&
        previous.Result && !*previous.Result && !previous.Read.Pending() &&
        previous.BasisWrites != cache->Costs().BasisWrites) {
      NativeLookups_.erase(found);
      found = NativeLookups_.end();
    }
  }
  if (found == NativeLookups_.end()) {
    if (PendingBasisLookups() >= kCandidateWindow) { return nullptr; }
    constexpr size_t kLookupsMost = 256;
    if (NativeLookups_.size() >= kLookupsMost) {
      const auto idle = std::ranges::find_if(NativeLookups_, [](const auto &entry) {
        return entry.second->Task == Tasks::kNoTask &&
               entry.second->Stage != NativeLookup::Phase::Bind && !entry.second->Read.Pending();
      });
      if (idle == NativeLookups_.end()) { return nullptr; }
      NativeLookups_.erase(idle);
    }
    auto lookup = std::make_unique<NativeLookup>();
    lookup->BasisWrites = cache->Costs().BasisWrites;
    auto *const output = lookup.get();
    StartNativeLookup(cache, request, *output);
    NativeLookups_.emplace(request, std::move(lookup));
    return output->Task == Tasks::kNoTask ? output : nullptr;
  }
  auto &lookup = *found->second;
  if (lookup.Task != Tasks::kNoTask) {
    if (!Pool_->TakeCompletion(lookup.Task)) { return nullptr; }
    lookup.Task = Tasks::kNoTask;
  }
  if (!lookup.Result) {
    const auto *kind = std::get_if<Generators::StructureBakeErrorKind>(&lookup.Result.error());
    if (kind != nullptr && *kind == Generators::StructureBakeErrorKind::ArtifactBusy) {
      return RetryNativeLookup(cache, request, lookup);
    }
  }
  if (lookup.Result && lookup.Read.Pending()) { return RetryNativeLookup(cache, request, lookup); }
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
  if (PendingBasisLookups() >= kCandidateWindow) { return false; }
  lookup.Read = Generators::PreparedBuildingAssets::BasisRead{};
  lookup.Stage = NativeLookup::Phase::Content;
  lookup.ContentKey = key;
  lookup.StreetDigest = *digest;
  StartNativeLookup(cache, selected.RequestKey, lookup);
  return false;
}
}
