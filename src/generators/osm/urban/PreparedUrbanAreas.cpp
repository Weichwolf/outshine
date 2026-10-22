#include "PreparedUrbanAreas.h"
#include "AssetSourceRecipe.h"
#include "SourceSet.h"
#include <algorithm>
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
    : Cache_(std::move(cache)),
      Recipe_(std::move(recipe)),
      Compute_(compute),
      Prepared_(std::move(prepared)),
      Budget_(budget) {}

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
                         Budget budget) {
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
  std::filesystem::create_directories(directory, error);
  if (error) { return std::unexpected("could not create native urban-area cache"); }
  auto cache = AssetCache::Open((std::filesystem::path(directory) / "assets.sqlite").string());
  if (!cache) { return std::unexpected("could not open native urban-area cache"); }
  auto recipe = Generators::AssetSourceRecipe(
      "prepared-urban-area-1-parent5", sources, std::array{Data::DataKind::VectorMap});
  return std::shared_ptr<PreparedUrbanAreas>(new PreparedUrbanAreas(
      std::move(*cache), std::move(recipe), compute, std::move(prepared), budget));
}

PreparedUrbanAreas::~PreparedUrbanAreas() {
  Cancel();
  for (const auto &[key, work] : Pending_) {
    if (work->Task != Tasks::kNoTask) { Compute_.Wait(work->Task); }
  }
}

void PreparedUrbanAreas::Cancel() noexcept {
  Stop_.request_stop();
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
  std::erase_if(Pending_, [this](const auto &entry) {
    const auto &work = *entry.second;
    if (std::ranges::any_of(work.Owners, [](const auto &held) { return !held.expired(); })) {
      return false;
    }
    return work.Task == Tasks::kNoTask || Compute_.TakeCompletion(work.Task);
  });
}

std::expected<PreparedUrbanAreas::Delivery, std::string>
PreparedUrbanAreas::Acquire(TilePool &tiles, const Request &request) {
  if (Stop_.stop_requested()) { return std::unexpected("urban-area preparation cancelled"); }
  const Definition input{
      .Owner = request.Owner_, .Schema = request.Schema_, .Recipe = request.Recipe_};
  const auto key = RequestKey(input);
  if (key.empty() || !request.Demand_) { return std::unexpected("invalid urban-area request"); }
  if (Pending_.size() >= Budget_.PendingMost) { RetireUnused(); }
  const auto found = Pending_.find(key);
  if (found != Pending_.end()) {
    const auto work = found->second;
    Observe(*work, request.Demand_);
    auto result = Advance(tiles, key, work);
    if (!result || result->Status == State::Ready || result->Status == State::Absent) {
      Pending_.erase(key);
    }
    return result;
  }
  if (Pending_.size() >= Budget_.PendingMost) {
    ++Deferred_;
    return Delivery{};
  }
  auto work = std::make_shared<Work>();
  work->Input = input;
  Observe(*work, request.Demand_);
  work->Task = Compute_.Post([this, work, key] {
    work->Result = Stop_.stop_requested()
                       ? std::unexpected(std::string("urban-area preparation cancelled"))
                       : Load(key, work->Input);
  });
  if (work->Task == Tasks::kNoTask) {
    return std::unexpected("urban-area lookup task was not accepted");
  }
  Pending_.emplace(key, std::move(work));
  return Delivery{};
}

std::expected<PreparedUrbanAreas::Delivery, std::string> PreparedUrbanAreas::Advance(
    TilePool &tiles, const std::string &key, const std::shared_ptr<Work> &work) {
  if (work->Task != Tasks::kNoTask) {
    if (!Compute_.TakeCompletion(work->Task)) { return Delivery{}; }
    work->Task = Tasks::kNoTask;
    if (!work->Result) { return std::unexpected(work->Result.error()); }
    if (*work->Result) {
      return Delivery{.Status = State::Ready, .Product = std::move(*work->Result)};
    }
    if (work->Stage == Work::Phase::Generate) {
      return std::unexpected("urban-area generation returned no complete asset");
    }
    work->Stage = Work::Phase::Source;
  }
  auto sources = GatherSources(tiles, *work);
  if (!sources || sources->Status != State::Ready) { return sources; }
  work->Stage = Work::Phase::Generate;
  work->Task = Compute_.Post([this, work, key] { work->Result = Generate(key, *work); });
  if (work->Task == Tasks::kNoTask) {
    return std::unexpected("urban-area generation task was not accepted");
  }
  return Delivery{};
}

std::expected<PreparedUrbanAreas::Delivery, std::string>
PreparedUrbanAreas::GatherSources(TilePool &tiles, Work &work) {
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
      ++SourcePolls_;
      auto source = Prepared_->Acquire(
          tiles, {.X = static_cast<int>(at.X), .Y = static_cast<int>(at.Y)}, *work.Layouts[index]);
      if (!source) { return std::unexpected(std::string(source.error())); }
      switch (source->Status) {
        case PreparedOsmTiles::State::Absent:
          return Delivery{.Status = State::Absent, .Product = {}};
        case PreparedOsmTiles::State::Refused: refused = true; break;
        case PreparedOsmTiles::State::Pending: pending = true; break;
        case PreparedOsmTiles::State::Ready:
          if (!source->Product) { return std::unexpected("ready urban source has no product"); }
          work.Sources[index] = std::move(source->Product);
          break;
      }
    }
  }
  if (refused) { return Delivery{.Status = State::Refused, .Product = {}}; }
  if (pending) { return Delivery{}; }
  return Delivery{.Status = State::Ready, .Product = {}};
}

PreparedUrbanAreas::Counters PreparedUrbanAreas::Costs() const noexcept {
  return {.Hits = Hits_.load(),
          .Misses = Misses_.load(),
          .Generations = Generations_.load(),
          .Writes = Writes_.load(),
          .ReadBytes = ReadBytes_.load(),
          .SourcePolls = SourcePolls_.load(),
          .GeneratedCells = GeneratedCells_.load(),
          .Deferred = Deferred_.load()};
}
}
