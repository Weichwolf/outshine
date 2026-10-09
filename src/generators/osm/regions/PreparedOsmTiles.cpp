#include "PreparedOsmTiles.h"
#include "AssetSourceRecipe.h"
#include "BinaryValueArchive.h"
#include "Sha256.h"
#include "SourceSet.h"
#include <array>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <ratio>
#include <limits>
#include <string>
#include <system_error>
#include <utility>

namespace outshine::Generators::Osm {
PreparedOsmTiles::PreparedOsmTiles(std::unique_ptr<AssetCache> cache,
                                   Tasks &compute,
                                   std::string recipe)
    : Cache_(std::move(cache)), Compute_(compute), Recipe_(std::move(recipe)) {}

std::expected<std::shared_ptr<PreparedOsmTiles>, std::string> PreparedOsmTiles::Open(
    const std::string &directory, const Data::SourceSet &sources, Tasks &compute) {
  std::error_code error;
  std::filesystem::create_directories(directory, error);
  if (error) { return std::unexpected("could not create native OSM tile cache"); }
  auto cache = AssetCache::Open((std::filesystem::path(directory) / "assets.sqlite").string());
  if (!cache) { return std::unexpected("could not open native OSM tile cache"); }
  const auto recipe = Generators::AssetSourceRecipe(
      "prepared-osm-tile-1", sources, std::array{Data::DataKind::VectorMap});
  return std::shared_ptr<PreparedOsmTiles>(
      new PreparedOsmTiles(std::move(*cache), compute, recipe));
}

PreparedOsmTiles::~PreparedOsmTiles() {
  for (const auto &[key, work] : Pending_) {
    if (work->Task != Tasks::kNoTask) { Compute_.Wait(work->Task); }
  }
}

void PreparedOsmTiles::Observe(Work &work, const std::shared_ptr<const uint8_t> &owner) {
  std::erase_if(work.Owners, [](const auto &held) { return held.expired(); });
  if (std::ranges::none_of(work.Owners, [&](const auto &held) { return held.lock() == owner; })) {
    work.Owners.push_back(owner);
  }
}

void PreparedOsmTiles::RetireUnused() {
  std::erase_if(Pending_, [this](const auto &entry) {
    const auto &work = *entry.second;
    if (std::ranges::any_of(work.Owners, [](const auto &held) { return !held.expired(); })) {
      return false;
    }
    return work.Task == Tasks::kNoTask || Compute_.TakeCompletion(work.Task);
  });
}

std::string PreparedOsmTiles::RequestKey(TileAt at, const Layout &layout) const {
  BinaryValueWriter out(4096);
  if (!out.Text(Recipe_) || !out(layout.Zoom, at.X, at.Y, layout.Schema)) { return {}; }
  for (const auto &layer : layout.Layers) {
    if (!out.Text(layer)) { return {}; }
  }
  return Sha256Hex(out.Out.Bytes().data(), out.Out.Bytes().size());
}

std::expected<PreparedOsmTiles::Delivery, std::string_view>
PreparedOsmTiles::Acquire(TilePool &tiles, TileAt at, const OsmField &layout) {
  if (layout.Zoom_ < 0 || layout.Zoom_ >= std::numeric_limits<int>::digits || at.X < 0 ||
      at.Y < 0 ||
      std::cmp_greater_equal(at.X, uint64_t{1} << static_cast<unsigned>(layout.Zoom_)) ||
      std::cmp_greater_equal(at.Y, uint64_t{1} << static_cast<unsigned>(layout.Zoom_))) {
    return std::unexpected("invalid native OSM tile address");
  }
  const Layout definition{.Zoom = layout.Zoom_, .Schema = layout.Schema_, .Layers = layout.Layers_};
  const auto request = RequestKey(at, definition);
  if (request.empty()) { return std::unexpected("invalid native OSM tile request"); }
  if (Pending_.size() >= kPendingMost) { RetireUnused(); }
  const auto found = Pending_.find(request);
  if (found == Pending_.end()) {
    if (Pending_.size() >= kPendingMost) { return Delivery{}; }
    auto work = std::make_shared<Work>();
    work->Definition = definition;
    Observe(*work, layout.Demand_);
    work->Task = Compute_.Post([this, work, request] {
      const auto began = std::chrono::steady_clock::now();
      work->Result = Load(request, work->Definition);
      ReadMs_.fetch_add(
          std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began)
              .count());
    });
    if (work->Task == Tasks::kNoTask) {
      return std::unexpected("native OSM tile lookup was not accepted");
    }
    Pending_.emplace(request, std::move(work));
    return Delivery{};
  }
  const auto &work = found->second;
  Observe(*work, layout.Demand_);
  if (work->Task != Tasks::kNoTask) {
    if (!Compute_.TakeCompletion(work->Task)) { return Delivery{}; }
    work->Task = Tasks::kNoTask;
    if (!work->Result) { return std::unexpected(std::string_view(work->Result.error())); }
    if (*work->Result) {
      auto product = std::move(*work->Result);
      Pending_.erase(found);
      return Delivery{.Status = State::Ready, .Product = std::move(product)};
    }
    work->Stage = Work::Phase::Source;
  }
  if (!work->Result) { return std::unexpected(std::string_view(work->Result.error())); }
  if (work->Stage != Work::Phase::Source) { return Delivery{}; }
  TilePool::Landing landing;
  const auto status =
      tiles.Bytes(Data::Fetch(Data::DataKind::VectorMap,
                              Data::Address::At({.Zoom = work->Definition.Zoom,
                                                 .X = static_cast<uint32_t>(at.X),
                                                 .Y = static_cast<uint32_t>(at.Y)})),
                  &landing);
  switch (status) {
    case TilePool::Reply::Absent:
    case TilePool::Reply::Undeclared:
      Pending_.erase(found);
      return Delivery{.Status = State::Absent, .Product = {}};
    case TilePool::Reply::Refused: return Delivery{.Status = State::Refused, .Product = {}};
    case TilePool::Reply::Pending:
    case TilePool::Reply::Deferred: return Delivery{};
    default: break;
  }
  work->Stage = Work::Phase::Generate;
  work->Task = Compute_.Post([this, work, at, request, landing = std::move(landing)] mutable {
    const auto began = std::chrono::steady_clock::now();
    work->Result = Generate(at, std::move(landing), request, work->Definition);
    GenerationMs_.fetch_add(
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began)
            .count());
  });
  if (work->Task == Tasks::kNoTask) {
    work->Result = std::unexpected("native OSM tile generation was not accepted");
    return std::unexpected(std::string_view(work->Result.error()));
  }
  return Delivery{};
}

PreparedOsmTiles::Counters PreparedOsmTiles::Costs() const noexcept {
  return {.Hits = Hits_.load(),
          .Misses = Misses_.load(),
          .Writes = Writes_.load(),
          .ReadBytes = ReadBytes_.load(),
          .ReadMs = ReadMs_.load(),
          .GenerationMs = GenerationMs_.load()};
}
}
