#include "PreparedOsmTiles.h"
#include "MvtSchema.h"
#include "AssetSourceRecipe.h"
#include "BinaryValueArchive.h"
#include "Sha256.h"
#include "SourceSet.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <ratio>
#include <string>
#include <system_error>
#include <utility>

namespace outshine::Generators::Osm {
PreparedOsmTiles::PreparedOsmTiles(std::unique_ptr<AssetCache> cache,
                                   std::shared_ptr<Data::SourceSession> sources,
                                   Data::SourceTransport &transport,
                                   Tasks &compute,
                                   std::string recipe,
                                   Budget budget)
    : Cache_(std::move(cache)),
      Sources_(std::move(sources)),
      Transport_(transport),
      Compute_(compute),
      Recipe_(std::move(recipe)),
      Budget_(budget) {}

std::expected<std::shared_ptr<PreparedOsmTiles>, std::string>
PreparedOsmTiles::Open(const std::string &directory,
                       std::shared_ptr<Data::SourceSession> sources,
                       Data::SourceTransport &transport,
                       Tasks &compute) {
  return Open(directory, std::move(sources), transport, compute, {});
}

std::expected<std::shared_ptr<PreparedOsmTiles>, std::string>
PreparedOsmTiles::Open(const std::string &directory,
                       std::shared_ptr<Data::SourceSession> sources,
                       Data::SourceTransport &transport,
                       Tasks &compute,
                       Budget budget) {
  if (!sources) { return std::unexpected("native OSM preparation requires configured sources"); }
  if (budget.PendingMost == 0 || budget.PublicationBytesMost == 0) {
    return std::unexpected("invalid native OSM publication budget");
  }
  std::error_code error;
  std::filesystem::create_directories(directory, error);
  if (error) { return std::unexpected("could not create native OSM tile cache"); }
  auto cache = AssetCache::Open((std::filesystem::path(directory) / "assets.sqlite").string());
  if (!cache) { return std::unexpected("could not open native OSM tile cache"); }
  const auto recipe = Generators::AssetSourceRecipe(
      "prepared-osm-tile-3", sources->Sources(), std::array{Data::DataKind::VectorMap});
  if (recipe.empty()) { return std::unexpected("OSM source metadata cannot be encoded"); }
  return std::shared_ptr<PreparedOsmTiles>(new PreparedOsmTiles(
      std::move(*cache), std::move(sources), transport, compute, recipe, budget));
}

PreparedOsmTiles::~PreparedOsmTiles() {
  for (const auto &[key, work] : Pending_) { work->Cancelled.store(true); }
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

void PreparedOsmTiles::Forget(Work &work) {
  if (work.Generating) {
    --Generating_;
    work.Generating = false;
  }
  SetPublication(work, std::nullopt);
}

bool PreparedOsmTiles::CompleteTask(Work &work) {
  if (work.Task == Tasks::kNoTask) { return true; }
  if (!Compute_.TakeCompletion(work.Task)) { return false; }
  work.Task = Tasks::kNoTask;
  if (work.Generating) {
    --Generating_;
    work.Generating = false;
  }
  if (work.Stage == Work::Phase::Generate && work.Publication) {
    work.Stage = Work::Phase::Publish;
  }
  return true;
}

void PreparedOsmTiles::RetireUnused() {
  NextRetire_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(1);
  std::erase_if(Pending_, [this](const auto &entry) {
    auto &work = *entry.second;
    if (std::ranges::any_of(work.Owners, [](const auto &held) { return !held.expired(); })) {
      return false;
    }
    work.Cancelled.store(true);
    if (!CompleteTask(work)) { return false; }
    Forget(work);
    return true;
  });
}

std::string PreparedOsmTiles::RequestKey(TileAt at, const Layout &layout) const {
  BinaryValueWriter out(4096);
  if (!out.Text(Recipe_) || !out(layout.Zoom, at.X, at.Y, layout.Schema)) { return {}; }
  for (const auto &layer : layout.Layers) {
    if (!out.Text(layer)) { return {}; }
    if (layout.Schema == MvtSchema::OpenMapTiles && (layer == "land" || layer == "sites") &&
        !out.Text(MvtLayerName(layout.Schema, layer))) {
      return {};
    }
  }
  return Sha256Hex(out.Out.Bytes().data(), out.Out.Bytes().size());
}

std::expected<PreparedOsmTiles::SourceDelivery, std::string_view>
PreparedOsmTiles::Acquire(TileAt at, const OsmField &layout) {
  if (layout.Zoom_ < 0 || layout.Zoom_ >= std::numeric_limits<int>::digits || at.X < 0 ||
      at.Y < 0 ||
      std::cmp_greater_equal(at.X, uint32_t{1} << static_cast<unsigned>(layout.Zoom_)) ||
      std::cmp_greater_equal(at.Y, uint32_t{1} << static_cast<unsigned>(layout.Zoom_))) {
    return std::unexpected("invalid native OSM tile address");
  }
  const Layout definition{.Zoom = layout.Zoom_, .Schema = layout.Schema_, .Layers = layout.Layers_};
  const auto request = RequestKey(at, definition);
  if (request.empty()) { return std::unexpected("invalid native OSM tile request"); }
  if ((Pending_.size() >= Budget_.PendingMost ||
       PublicationBytes_.load() >= Budget_.PublicationBytesMost ||
       std::cmp_greater_equal(Generating_, Compute_.Threads())) &&
      std::chrono::steady_clock::now() >= NextRetire_) {
    RetireUnused();
  }
  const auto found = Pending_.find(request);
  if (found == Pending_.end()) {
    if (Pending_.size() >= Budget_.PendingMost) { return SourceDelivery{}; }
    auto work = std::make_shared<Work>();
    work->Definition = definition;
    Observe(*work, layout.Demand_);
    work->Task = Compute_.Post([this, work, request] { Lookup(request, *work); });
    if (work->Task == Tasks::kNoTask) {
      return std::unexpected("native OSM tile lookup was not accepted");
    }
    Pending_.emplace(request, std::move(work));
    return SourceDelivery{};
  }
  const auto work = found->second;
  if (work->Cancelled.load()) {
    if (CompleteTask(*work)) {
      Forget(*work);
      Pending_.erase(found);
    }
    return SourceDelivery{};
  }
  Observe(*work, layout.Demand_);
  auto result = Advance(at, request, work);
  if (result && (result->Status == State::Ready || result->Status == State::Absent)) {
    Forget(*work);
    Pending_.erase(request);
  }
  return result;
}

std::expected<PreparedOsmTiles::SourceDelivery, std::string_view> PreparedOsmTiles::Advance(
    TileAt at, const std::string &request, const std::shared_ptr<Work> &work) {
  if (!CompleteTask(*work)) { return SourceDelivery{}; }
  if (!work->Result) { return std::unexpected(std::string_view(work->Result.error())); }
  if (*work->Result) {
    return SourceDelivery{.Status = State::Ready, .Product = std::move(*work->Result)};
  }
  if (work->Stage == Work::Phase::Lookup || work->Stage == Work::Phase::Publish ||
      work->Stage == Work::Phase::Restore) {
    if (std::chrono::steady_clock::now() < work->RetryAt) { return SourceDelivery{}; }
    work->Task = Compute_.Post([this, work, request] {
      if (work->Stage == Work::Phase::Lookup) {
        Lookup(request, *work);
      } else {
        const auto began = std::chrono::steady_clock::now();
        Publish(request, *work);
        PublicationMs_.fetch_add(
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began)
                .count());
      }
    });
    if (work->Task == Tasks::kNoTask) {
      work->Result = std::unexpected("native OSM publication task was not accepted");
      return std::unexpected(std::string_view(work->Result.error()));
    }
    return SourceDelivery{};
  }
  if (work->Stage != Work::Phase::Source) {
    return std::unexpected("native OSM generation returned no complete asset");
  }
  if (std::cmp_greater_equal(Generating_, Compute_.Threads()) ||
      PublicationBytes_.load() >= Budget_.PublicationBytesMost) {
    return SourceDelivery{};
  }
  if (work->InputState == Data::SourceDelivery::State::Vacant ||
      work->InputState == Data::SourceDelivery::State::Undeclared) {
    return SourceDelivery{.Status = State::Absent, .Product = {}};
  }
  if (work->InputState == Data::SourceDelivery::State::Refused) {
    return SourceDelivery{.Status = State::Refused, .Product = {}};
  }
  work->Task = Compute_.Post([this, work, at, request] { PollSource(at, request, *work); });
  if (work->Task == Tasks::kNoTask) {
    work->Result = std::unexpected("native OSM tile generation was not accepted");
    return std::unexpected(std::string_view(work->Result.error()));
  }
  work->Generating = true;
  ++Generating_;
  return SourceDelivery{};
}

void PreparedOsmTiles::PollSource(TileAt at, const std::string &request, Work &work) {
  if (work.Cancelled.load()) {
    work.Input.reset();
    return;
  }
  if (!work.Input) {
    auto read = Data::SourceSession::Request(
        Sources_,
        Data::SourceRequest(Data::DataKind::VectorMap,
                            Data::Address::At({.Zoom = work.Definition.Zoom,
                                               .X = static_cast<uint32_t>(at.X),
                                               .Y = static_cast<uint32_t>(at.Y)})),
        Transport_);
    if (!read) {
      work.Result = std::unexpected(std::move(read.error()));
      return;
    }
    work.Input.emplace(std::move(*read));
  }
  auto delivery = work.Input->Poll();
  work.InputState = delivery.Where();
  if (work.InputState == Data::SourceDelivery::State::Pending) { return; }
  work.Input.reset();
  if (work.InputState != Data::SourceDelivery::State::Delivered) {
    if (work.InputState == Data::SourceDelivery::State::Consumed) {
      work.Result = std::unexpected("native OSM source reader returned no payload");
    }
    return;
  }
  auto landing = delivery.Take();
  if (!landing) {
    work.Result = std::unexpected("native OSM source reader returned no payload");
    return;
  }
  work.Stage = Work::Phase::Generate;
  const auto began = std::chrono::steady_clock::now();
  work.Result = Generate(at, std::move(*landing), request, work);
  GenerationMs_.fetch_add(
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count());
}

PreparedOsmTiles::Counters PreparedOsmTiles::Costs() const noexcept {
  return {.Hits = Hits_.load(),
          .Misses = Misses_.load(),
          .Writes = Writes_.load(),
          .ReadBytes = ReadBytes_.load(),
          .ReadMs = ReadMs_.load(),
          .GenerationMs = GenerationMs_.load(),
          .Generations = Generations_.load(),
          .PublicationAttempts = PublicationAttempts_.load(),
          .PublicationDeferred = PublicationDeferred_.load(),
          .PublicationBytes = PublicationBytes_.load(),
          .PublicationPeakBytes = PublicationPeakBytes_.load(),
          .PublicationReadBytes = PublicationReadBytes_.load(),
          .PublicationMs = PublicationMs_.load()};
}
}
