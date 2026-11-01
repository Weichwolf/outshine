#include "PreparedUrbanWorker.h"
#include "AssetSourceRecipe.h"
#include "SourceSet.h"
#include <algorithm>
#include <atomic>
#include <mutex>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>
#include <utility>

namespace outshine::Generators::Osm {
PreparedUrbanAreas::PreparedUrbanAreas(std::unique_ptr<AssetCache> cache,
                                       std::string recipe,
                                       Tasks &compute,
                                       std::shared_ptr<PreparedOsmTiles> prepared,
                                       Budget budget)
    : Compute_(compute),
      Prepared_(std::move(prepared)),
      Worker_(std::make_shared<Worker>(std::move(cache), std::move(recipe), budget)) {}

PreparedUrbanAreas::Worker::Worker(std::unique_ptr<AssetCache> cache,
                                   std::string recipe,
                                   Budget budget)
    : Cache(std::move(cache)), Recipe(std::move(recipe)), Limits(budget) {}

PreparedUrbanAreas::Worker::Job::Job(std::shared_ptr<Worker> owner, std::shared_ptr<Work> work)
    : Owner(std::move(owner)), Pending(std::move(work)) {
  Pending->Running.store(true, std::memory_order_relaxed);
}

PreparedUrbanAreas::Worker::Job::~Job() {
  {
    const std::scoped_lock lock(Owner->CompletionMutex);
    Pending->Running.store(false, std::memory_order_release);
    ++Owner->CompletionGeneration;
  }
  Owner->Completed.notify_all();
}

std::expected<std::shared_ptr<PreparedUrbanAreas>, std::string>
PreparedUrbanAreas::Open(const std::string &directory,
                         const Data::SourceSet &sources,
                         Tasks &compute,
                         std::shared_ptr<PreparedOsmTiles> prepared) {
  return Open(directory, sources, compute, std::move(prepared), Budget{});
}

std::expected<std::shared_ptr<PreparedUrbanAreas>, std::string>
PreparedUrbanAreas::Open(const std::string &directory,
                         const Data::SourceSet &sources,
                         Tasks &compute,
                         std::shared_ptr<PreparedOsmTiles> prepared,
                         Budget budget,
                         AssetCache::Runtime runtime) {
  constexpr size_t maxPending = 16;
  constexpr size_t maxCells = 4194304;
  constexpr size_t maxSourceBytes = size_t{128} * 1024u * 1024u;
  constexpr auto maxTime = std::chrono::seconds(60);
  if (!prepared || budget.PendingMost == 0 || budget.PendingMost > maxPending ||
      budget.SourceBytesMost == 0 || budget.SourceBytesMost > maxSourceBytes ||
      budget.CellsMost == 0 || budget.CellsMost > maxCells || budget.SourceVerticesMost == 0 ||
      budget.FeaturesMost == 0 || budget.GenerationTime < std::chrono::milliseconds::zero() ||
      budget.GenerationTime > maxTime) {
    return std::unexpected("invalid urban-area preparation budget or source service");
  }
  std::error_code error;
  if (runtime.Workers == nullptr) { std::filesystem::create_directories(directory, error); }
  if (error) { return std::unexpected("could not create native urban-area cache"); }
  auto cache = AssetCache::Open((std::filesystem::path(directory) / "assets.sqlite").string(),
                                std::move(runtime));
  if (!cache) { return std::unexpected("could not open native urban-area cache"); }
  auto recipe = Generators::AssetSourceRecipe(
      "prepared-urban-area-1-parent5", sources, std::array{Data::DataKind::VectorMap});
  if (recipe.empty()) { return std::unexpected("urban source metadata cannot be encoded"); }
  return std::shared_ptr<PreparedUrbanAreas>(new PreparedUrbanAreas(
      std::move(*cache), std::move(recipe), compute, std::move(prepared), budget));
}

PreparedUrbanAreas::~PreparedUrbanAreas() {
  Cancel();
  (void)Compute_.PostDetached([worker = std::move(Worker_),
                               prepared = std::move(Prepared_),
                               pending = std::move(Pending_)] {
    (void)worker;
    (void)prepared;
    (void)pending;
  });
}

void PreparedUrbanAreas::Cancel() noexcept {
  Worker_->Stop.request_stop();
}

Data::TileId PreparedUrbanAreas::Parent(Data::TileId owner) noexcept {
  constexpr int parentZoom = 5;
  const auto zoom = std::min(owner.Zoom, parentZoom);
  const auto shift = static_cast<unsigned>(owner.Zoom - zoom);
  return {.Zoom = zoom, .X = owner.X >> shift, .Y = owner.Y >> shift};
}

void PreparedUrbanAreas::Observe(Work &work, const std::shared_ptr<const uint8_t> &owner) {
  std::erase_if(work.Owners, [](const auto &held) { return held.expired(); });
  if (std::ranges::none_of(work.Owners, [&](const auto &held) { return held.lock() == owner; })) {
    work.Owners.push_back(owner);
  }
}

void PreparedUrbanAreas::RetireUnused() {
  std::erase_if(Pending_, [](const auto &entry) {
    const auto &work = *entry.second;
    if (std::ranges::any_of(work.Owners, [](const auto &held) { return !held.expired(); })) {
      return false;
    }
    return !work.Running.load(std::memory_order_acquire);
  });
}

std::expected<PreparedUrbanAreas::SourceDelivery, std::string>
PreparedUrbanAreas::Acquire(const Request &request) {
  if (Worker_->Stop.stop_requested()) {
    return std::unexpected("urban-area preparation cancelled");
  }
  const Definition input{
      .Owner = request.Owner_, .Schema = request.Schema_, .Recipe = request.Recipe_};
  const auto key = Worker_->RequestKey(input);
  if (key.empty() || !request.Demand_) { return std::unexpected("invalid urban-area request"); }
  if (Pending_.size() >= Worker_->Limits.PendingMost) { RetireUnused(); }
  const auto found = Pending_.find(key);
  if (found != Pending_.end()) {
    const auto work = found->second;
    Observe(*work, request.Demand_);
    auto result = Advance(key, work);
    if (!result || result->Status == State::Ready || result->Status == State::Absent) {
      Pending_.erase(key);
    }
    return result;
  }
  if (Pending_.size() >= Worker_->Limits.PendingMost) {
    ++Worker_->Deferred;
    return SourceDelivery{};
  }
  auto work = std::make_shared<Work>();
  work->Input = input;
  Observe(*work, request.Demand_);
  if (!Start(key, work)) { return std::unexpected("urban-area lookup task was not accepted"); }
  Pending_.emplace(key, std::move(work));
  return SourceDelivery{};
}

bool PreparedUrbanAreas::Start(const std::string &key, const std::shared_ptr<Work> &work) {
  const auto job = std::make_shared<Worker::Job>(Worker_, work);
  return Compute_.PostDetached([job, key] {
    auto &owner = *job->Owner;
    auto &pending = *job->Pending;
    if (owner.Stop.stop_requested()) {
      pending.Result = std::unexpected(std::string("urban-area preparation cancelled"));
    } else if (pending.Stage == Work::Phase::Lookup) {
      pending.Result = owner.Load(key, pending.Input);
    } else {
      pending.Result = owner.Generate(key, pending);
    }
  });
}

std::expected<PreparedUrbanAreas::SourceDelivery, std::string>
PreparedUrbanAreas::Advance(const std::string &key, const std::shared_ptr<Work> &work) {
  if (work->Running.load(std::memory_order_acquire)) { return SourceDelivery{}; }
  if (work->Stage != Work::Phase::Source) {
    if (!work->Result) { return std::unexpected(work->Result.error()); }
    if (work->Result->Area) {
      return SourceDelivery{.Status = State::Ready,
                            .Product = std::move(work->Result->Area),
                            .AssetKey = std::move(work->Result->Key)};
    }
    if (work->Stage == Work::Phase::Generate) {
      return std::unexpected("urban-area generation returned no complete asset");
    }
    work->Stage = Work::Phase::Source;
  }
  auto sources = GatherSources(*work);
  if (!sources || sources->Status != State::Ready) { return sources; }
  work->Stage = Work::Phase::Generate;
  if (!Start(key, work)) { return std::unexpected("urban-area generation task was not accepted"); }
  return SourceDelivery{};
}

std::expected<PreparedUrbanAreas::SourceDelivery, std::string>
PreparedUrbanAreas::GatherSources(Work &work) {
  const auto parent = Parent(work.Input.Owner);
  if (work.SourceCount == 0) {
    const std::array<std::string, 7> layers{
        "sites", "land", "park", "water_polygons", "water_lines", "streets", "aeroway"};
    const std::array<std::string, 1> parentLayers{"sites"};
    work.Layouts[0] = std::make_unique<OsmField>(work.Input.Owner.Zoom, layers, work.Input.Schema);
    work.SourceCount = parent == work.Input.Owner ? 1u : 2u;
    if (work.SourceCount == 2) {
      work.Layouts[1] = std::make_unique<OsmField>(parent.Zoom, parentLayers, work.Input.Schema);
    }
  }
  bool pending = false;
  bool refused = false;
  for (size_t index = 0; index < work.SourceCount; ++index) {
    if (!work.Sources[index]) {
      const auto at = index == 0 ? work.Input.Owner : parent;
      ++Worker_->SourcePolls;
      auto source = Prepared_->Acquire({.X = static_cast<int>(at.X), .Y = static_cast<int>(at.Y)},
                                       *work.Layouts[index]);
      if (!source) { return std::unexpected(std::string(source.error())); }
      switch (source->Status) {
        case PreparedOsmTiles::State::Absent:
          return SourceDelivery{.Status = State::Absent, .Product = {}, .AssetKey = {}};
        case PreparedOsmTiles::State::Refused: refused = true; break;
        case PreparedOsmTiles::State::Pending: pending = true; break;
        case PreparedOsmTiles::State::Ready:
          if (!source->Product) { return std::unexpected("ready urban source has no product"); }
          work.Sources[index] = std::move(source->Product);
          break;
      }
    }
  }
  if (refused) { return SourceDelivery{.Status = State::Refused, .Product = {}, .AssetKey = {}}; }
  if (pending) { return SourceDelivery{}; }
  return SourceDelivery{.Status = State::Ready, .Product = {}, .AssetKey = {}};
}

PreparedUrbanAreas::Counters PreparedUrbanAreas::Costs() const noexcept {
  return {.Hits = Worker_->Hits.load(),
          .Misses = Worker_->Misses.load(),
          .Generations = Worker_->Generations.load(),
          .Writes = Worker_->Writes.load(),
          .ReadBytes = Worker_->ReadBytes.load(),
          .SourcePolls = Worker_->SourcePolls.load(),
          .GeneratedCells = Worker_->GeneratedCells.load(),
          .Deferred = Worker_->Deferred.load()};
}

bool PreparedUrbanAreas::AwaitProgress(double seconds) const {
  std::unique_lock lock(Worker_->CompletionMutex);
  bool running = false;
  for (const auto &[key, work] : Pending_) {
    if (work->Running.load(std::memory_order_acquire)) {
      running = true;
    } else if (work->Stage != Work::Phase::Source) {
      return true;
    }
  }
  if (!running) { return true; }
  if (!(seconds > 0.0)) { return false; }
  const auto generation = Worker_->CompletionGeneration;
  return Worker_->Completed.wait_for(
      lock, std::chrono::duration<double>(seconds), [this, generation] {
        return Worker_->CompletionGeneration != generation;
      });
}
}
