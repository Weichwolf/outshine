#include "StructureBuildQueue.h"
#include "PreparedBuildingAssets.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
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
struct StructureBuildQueue::NativeProgress {
  std::mutex Mutex;
  std::condition_variable Changed;
  uint64_t Generation = 0;
};

struct StructureBuildQueue::NativeLookup::Completion {
  Completion(std::shared_ptr<NativeLookup> value, std::shared_ptr<NativeProgress> progress)
      : Value(std::move(value)), Progress(std::move(progress)) {}

  ~Completion() {
    if (!Started) {
      Value->Result = std::unexpected(Generators::StructureBakeErrorKind::Cancelled);
    }
    {
      const std::scoped_lock lock(Progress->Mutex);
      Value->Finished.store(true, std::memory_order_release);
      ++Progress->Generation;
    }
    Progress->Changed.notify_all();
  }

  Completion(const Completion &) = delete;
  Completion &operator=(const Completion &) = delete;
  std::shared_ptr<NativeLookup> Value;
  std::shared_ptr<NativeProgress> Progress;
  bool Started = false;
};

bool StructureBuildQueue::AwaitNativeLookups(double seconds) const {
  if (!Context_.LookupProgress || !(seconds > 0.0)) { return false; }
  std::unique_lock lock(Context_.LookupProgress->Mutex);
  if (std::ranges::any_of(Context_.NativeLookups, [](const auto &entry) {
        return entry.second->Working && entry.second->Finished.load(std::memory_order_acquire);
      })) {
    return true;
  }
  const auto generation = Context_.LookupProgress->Generation;
  return Context_.LookupProgress->Changed.wait_for(
      lock, std::chrono::duration<double>(seconds), [&] {
        return Context_.LookupProgress->Generation != generation;
      });
}

void StructureBuildQueue::RetireNativeLookups() {
  for (const auto &entry : Context_.NativeLookups) {
    entry.second->Stopping.store(true, std::memory_order_relaxed);
  }
  if (Context_.NativeLookups.empty() && !Context_.NativeAssets) {
    Context_.LookupProgress.reset();
    return;
  }

  struct Retired {
    decltype(Context_.NativeLookups) Lookups;
    decltype(Context_.NativeAssets) Cache;
    std::shared_ptr<NativeProgress> Progress;
  };

  const auto retired = std::make_shared<Retired>(std::exchange(Context_.NativeLookups, {}),
                                                 std::move(Context_.NativeAssets),
                                                 std::move(Context_.LookupProgress));
  if (Pool_ != nullptr) {
    (void)Pool_->PostDetached([retired] { (void)retired; });
  }
}

bool StructureBuildQueue::AwaitSlice(double seconds) const {
  if (Pool_ == nullptr) { return false; }
  double wait = seconds;
  bool pendingGeometry = false;
  for (const QueuedBuild &build : Context_.Queue) {
    if (build.Finished && build.Task.Result().Status && !build.Task.Result().Tile) {
      pendingGeometry = true;
      wait = std::min(wait, build.Task.ResumeDelaySeconds());
    }
  }
  if (PendingBasisLookups() != 0) {
    const auto now = std::chrono::steady_clock::now();
    for (const auto &entry : Context_.NativeLookups) {
      const auto &lookup = *entry.second;
      if (!lookup.Working && lookup.Retries != 0) {
        const auto remaining = std::chrono::duration<double>(lookup.RetryAt - now).count();
        wait = std::min(wait, std::max(0.0, remaining));
      }
    }
    return AwaitNativeLookups(wait);
  }
  if (Context_.OriginalPreparation && Context_.OriginalPreparation->Running()) {
    return Context_.OriginalPreparation->AwaitSlice(wait);
  }
  for (const QueuedBuild &build : Context_.Queue) {
    if (build.Task.Running()) { return build.Task.AwaitCompletion(wait); }
  }
  return pendingGeometry && Pool_->AwaitCompletion(wait);
}

size_t StructureBuildQueue::PendingBasisLookups() const noexcept {
  return static_cast<size_t>(std::ranges::count_if(Context_.NativeLookups, [](const auto &entry) {
    return entry.second->Working || entry.second->Retries != 0;
  }));
}

void StructureBuildQueue::ReadNativeLookup(Generators::PreparedBuildingAssets &cache,
                                           const std::string &request,
                                           NativeLookup &lookup) {
  if (lookup.Stage == NativeLookup::Phase::Request) {
    lookup.BasisWrites = cache.Costs().BasisWrites;
    lookup.Result = cache.LoadBasisRequest(request);
    return;
  }
  if (!lookup.HeldBasis) {
    auto basis = cache.LoadBasis(lookup.ContentKey);
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
  lookup.Stage = NativeLookup::Phase::Bind;
  const auto bound = cache.BindBasisRequestIfUnbound(lookup.ContentKey, request);
  if (!bound) {
    lookup.Result = std::unexpected(bound.error());
    return;
  }
  lookup.BasisWrites = cache.Costs().BasisWrites;
  lookup.Result = cache.LoadBasisRequest(request);
  if (lookup.Result) {
    lookup.HeldBasis.reset();
    lookup.Stage = NativeLookup::Phase::Content;
  }
}

void StructureBuildQueue::StartNativeLookup(
    const std::shared_ptr<Generators::PreparedBuildingAssets> &cache,
    const std::string &request,
    const std::shared_ptr<NativeLookup> &lookup) {
  if (!Context_.LookupProgress) { Context_.LookupProgress = std::make_shared<NativeProgress>(); }
  lookup->Finished.store(false, std::memory_order_relaxed);
  lookup->Working = true;
  const auto completion =
      std::make_shared<NativeLookup::Completion>(lookup, Context_.LookupProgress);
  const bool accepted = Pool_->PostDetached([completion, cache, request] {
    completion->Started = true;
    auto &output = *completion->Value;
    if (output.Stopping.load(std::memory_order_relaxed)) {
      output.Result = std::unexpected(Generators::StructureBakeErrorKind::Cancelled);
      return;
    }
    const auto began = std::chrono::steady_clock::now();
    ReadNativeLookup(*cache, request, output);
    const auto *kind =
        output.Result ? nullptr
                      : std::get_if<Generators::StructureBakeErrorKind>(&output.Result.error());
    if (kind != nullptr && *kind == Generators::StructureBakeErrorKind::ArtifactBusy) {
      const auto exponent = std::min(output.Retries, 4U);
      output.Retries = std::min(output.Retries + 1U, 4U);
      output.RetryAt =
          std::chrono::steady_clock::now() + std::chrono::milliseconds(uint64_t{1} << exponent);
    } else {
      output.Retries = 0;
    }
    output.ReadMs +=
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  });
  if (!accepted) {
    lookup->Working = false;
    lookup->HeldBasis.reset();
    lookup->Retries = 0;
    lookup->Result = std::unexpected(Generators::StructureBakeErrorKind::Cancelled);
  }
}

StructureBuildQueue::NativeLookup *StructureBuildQueue::RetryNativeLookup(
    const std::shared_ptr<Generators::PreparedBuildingAssets> &cache,
    const std::string &request,
    const std::shared_ptr<NativeLookup> &lookup) {
  if (std::chrono::steady_clock::now() < lookup->RetryAt) { return nullptr; }
  const auto running = static_cast<size_t>(std::ranges::count_if(
      Context_.NativeLookups, [](const auto &entry) { return entry.second->Working; }));
  if (running >= kCandidateWindow) { return nullptr; }
  StartNativeLookup(cache, request, lookup);
  return lookup->Working ? nullptr : lookup.get();
}

StructureBuildQueue::NativeLookup *StructureBuildQueue::AdvanceNativeLookup(
    const std::shared_ptr<Generators::PreparedBuildingAssets> &cache, const std::string &request) {
  auto found = Context_.NativeLookups.find(request);
  if (found != Context_.NativeLookups.end()) {
    const auto &previous = *found->second;
    if (!previous.Working && previous.Retries == 0 &&
        previous.BasisWrites != cache->Costs().BasisWrites) {
      Context_.NativeLookups.erase(found);
      found = Context_.NativeLookups.end();
    }
  }
  if (found == Context_.NativeLookups.end()) {
    if (PendingBasisLookups() >= kCandidateWindow) { return nullptr; }
    constexpr size_t kLookupsMost = 256;
    if (Context_.NativeLookups.size() >= kLookupsMost) {
      const auto idle = std::ranges::find_if(Context_.NativeLookups, [](const auto &entry) {
        return !entry.second->Working && entry.second->Retries == 0;
      });
      if (idle == Context_.NativeLookups.end()) { return nullptr; }
      Context_.NativeLookups.erase(idle);
    }
    auto lookup = std::make_shared<NativeLookup>();
    lookup->BasisWrites = cache->Costs().BasisWrites;
    auto *const output = lookup.get();
    StartNativeLookup(cache, request, lookup);
    Context_.NativeLookups.emplace(request, std::move(lookup));
    return output->Working ? nullptr : output;
  }
  auto &lookup = *found->second;
  if (lookup.Working) {
    if (!lookup.Finished.load(std::memory_order_acquire)) { return nullptr; }
    lookup.Working = false;
  }
  if (lookup.Retries == 0 && lookup.BasisWrites != cache->Costs().BasisWrites) {
    Context_.NativeLookups.erase(found);
    return nullptr;
  }
  if (!lookup.Result) {
    const auto *kind = std::get_if<Generators::StructureBakeErrorKind>(&lookup.Result.error());
    if (kind != nullptr && *kind == Generators::StructureBakeErrorKind::ArtifactBusy) {
      return RetryNativeLookup(cache, request, found->second);
    }
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
  if (Context_.NativeAssets && Context_.NativeAssets != cache) {
    return std::unexpected(Generators::StructureBakeErrorKind::ArtifactInvalidProduct);
  }
  Context_.NativeAssets = cache;
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
  const auto found = Context_.NativeLookups.find(request);
  if (found == Context_.NativeLookups.end()) { return; }
  const auto &lookup = *found->second;
  if (!lookup.Working && lookup.Stage == NativeLookup::Phase::Request && lookup.Result &&
      *lookup.Result) {
    Context_.NativeLookups.erase(found);
  }
}

bool StructureBuildQueue::SelectNativeBasis(
    Ground::SurfacePreparation &stack,
    const ::outshine::Generators::Osm::BuildingField &prints,
    uint32_t tile,
    VectorSelection &selected) {
  const auto query = PollNativeBasis(stack, prints, tile);
  if (!query) {
    Context_.NativeFailure = query.error();
    return false;
  }
  if (query->Where == NativeBasisQuery::Phase::Unavailable) { return true; }
  selected.RequestKey = query->RequestKey;
  if (query->Where == NativeBasisQuery::Phase::Pending) { return false; }
  auto &lookup = *Context_.NativeLookups.at(selected.RequestKey);
  const auto &cache = stack.BuildingAssets();
  const auto &vectors = *stack.Vectors();
  const auto &region = vectors.Tiles()[tile];
  const Data::TileId address{
      .Zoom = region.Z, .X = static_cast<uint32_t>(region.X), .Y = static_cast<uint32_t>(region.Y)};
  if (!lookup.Result) {
    Context_.NativeFailure = lookup.Result.error();
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
    Context_.NativeFailure = Generators::StructureBakeErrorKind::ArtifactInvalidProduct;
    return false;
  }
  if (PendingBasisLookups() >= kCandidateWindow) { return false; }
  lookup.Stage = NativeLookup::Phase::Content;
  lookup.ContentKey = key;
  lookup.StreetDigest = *digest;
  StartNativeLookup(cache, selected.RequestKey, Context_.NativeLookups.at(selected.RequestKey));
  return false;
}
}
