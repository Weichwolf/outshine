#include "GroundPatchPreparation.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <optional>
#include <stop_token>
#include <variant>
#include <cstdint>
#include <expected>
#include <memory>
#include <string>
#include <utility>

namespace outshine {
struct GroundPatchPreparation::Work {
  enum class Status { Running, Deferred, CancelRequested, Complete };
  Factory Produce;
  std::optional<Result> Value;
  std::atomic<Status> State{Status::Running};
  std::chrono::steady_clock::time_point RetryAt;
  std::chrono::milliseconds Delay{1};

  void Finish() {
    const bool complete = Value.has_value();
    if (complete) { Produce = {}; }
    auto expected = Status::Running;
    if (State.compare_exchange_strong(expected,
                                      complete ? Status::Complete : Status::Deferred,
                                      std::memory_order_acq_rel,
                                      std::memory_order_acquire)) {
      return;
    }
    Value.emplace(std::unexpected("terrain sampling patch canceled"));
    Produce = {};
    State.store(Status::Complete, std::memory_order_release);
  }
};

namespace {
using Assets = Generators::PreparedTerrainAssets;

struct PatchStore {
  std::shared_ptr<Assets> Cache;
  std::string Key;
  Data::TileId At;
  std::shared_ptr<const Generators::GroundPatch> Patch;
  std::optional<Assets::PatchWrite> Write;

  std::expected<std::shared_ptr<const Generators::GroundPatch>, Assets::Failure>
  Advance(const std::stop_token &stop) {
    if (!Write) {
      if (!Patch) { return std::unexpected(std::string("terrain publication lost its patch")); }
      auto prepared = Cache->PreparePatch(Key, At, *Patch);
      if (!prepared) { return std::unexpected(std::move(prepared.error())); }
      Write.emplace(std::move(*prepared));
      Patch.reset();
    }
    return Cache->StorePatch(*Write, stop);
  }
};
}

void GroundPatchPreparation::Start(Factory factory) {
  Work_ = std::make_shared<Work>();
  Work_->Produce = std::exchange(factory, {});
  Post();
}

void GroundPatchPreparation::Post() {
  if (Pool_.PostDetached([work = Work_, stop = Stop_.get_token()] { Run(*work, stop); })) {
    return;
  }
  Work_->Value.emplace(std::unexpected("terrain sampling patch compute queue is closed"));
  Work_->Finish();
}

void GroundPatchPreparation::Run(Work &work, const std::stop_token &stop) {
  if (stop.stop_requested()) {
    work.Value.emplace(std::unexpected("terrain sampling patch canceled"));
  } else {
    auto result = work.Produce(stop);
    if (stop.stop_requested()) {
      work.Value.emplace(std::unexpected("terrain sampling patch canceled"));
    } else if (result) {
      work.Value.emplace(std::move(*result));
    } else {
      const auto *cache = std::get_if<AssetCacheError>(&result.error());
      if (cache != nullptr && *cache == AssetCacheError::Busy) {
        work.RetryAt = std::chrono::steady_clock::now() + work.Delay;
        work.Delay = std::min(work.Delay * 2, std::chrono::milliseconds(16));
      } else {
        work.Value.emplace(std::unexpected(Assets::DescribeFailure(result.error())));
      }
    }
  }
  work.Finish();
}

void GroundPatchPreparation::Resume() {
  if (Work_->State.load(std::memory_order_acquire) != Work::Status::Deferred ||
      std::chrono::steady_clock::now() < Work_->RetryAt) {
    return;
  }
  auto expected = Work::Status::Deferred;
  if (Work_->State.compare_exchange_strong(expected, Work::Status::Running)) { Post(); }
}

void GroundPatchPreparation::Cancel() {
  (void)Stop_.request_stop();
  if (!Work_) { return; }
  auto state = Work_->State.load(std::memory_order_acquire);
  while (state == Work::Status::Running || state == Work::Status::Deferred) {
    if (!Work_->State.compare_exchange_weak(state,
                                            Work::Status::CancelRequested,
                                            std::memory_order_acq_rel,
                                            std::memory_order_acquire)) {
      continue;
    }
    if (state == Work::Status::Deferred) { Post(); }
    return;
  }
}

GroundPatchPreparation::GroundPatchPreparation(
    Tasks &pool,
    std::shared_ptr<Generators::PreparedTerrainAssets> assets,
    std::string key,
    Generators::Tile region,
    int side)
    : Pool_(pool), Assets_(std::move(assets)), Key_(std::move(key)), Region_(region), Side_(side) {
  Start([assets = Assets_, key = Key_, region, side](const std::stop_token &) -> Attempt {
    if (!assets) { return std::unexpected(std::string("terrain sampling cache is unavailable")); }
    return assets->LoadPatch(key,
                             {.Zoom = region.Zoom(),
                              .X = static_cast<uint32_t>(region.X()),
                              .Y = static_cast<uint32_t>(region.Y())},
                             side);
  });
}

GroundPatchPreparation::~GroundPatchPreparation() {
  Cancel();
}

GroundPatchPreparation::Result GroundPatchPreparation::Advance(const GroundQuery &heights,
                                                               Generators::Snapped *how) {
  using Generators::Snapped;
  *how = Snapped::Waiting;
  if (Phase_ == Phase::Reading || Phase_ == Phase::Storing) {
    Resume();
    if (Work_->State.load(std::memory_order_acquire) != Work::Status::Complete) { return Result{}; }
    Result_ = Work_->Value ? std::move(*Work_->Value)
                           : Result(std::unexpected("terrain patch worker lost its result"));
    Work_.reset();
    if (!Result_) {
      Phase_ = Phase::Failed;
    } else {
      Phase_ = *Result_ ? Phase::Ready : Phase::Generating;
    }
  }
  if (Phase_ == Phase::Failed) { return Result_; }
  if (Phase_ == Phase::Ready) {
    *how = Snapped::Taken;
    return Result_;
  }
  auto patch = Generators::PatchOver(Region_, heights, how);
  if (!patch) { return Result{}; }
  if (patch->Side() != Side_) {
    Result_ = std::unexpected("terrain sampling patch grid changed during preparation");
    Phase_ = Phase::Failed;
    return Result_;
  }
  Phase_ = Phase::Storing;
  *how = Snapped::Waiting;
  auto store =
      std::make_shared<PatchStore>(PatchStore{.Cache = Assets_,
                                              .Key = Key_,
                                              .At = {.Zoom = Region_.Zoom(),
                                                     .X = static_cast<uint32_t>(Region_.X()),
                                                     .Y = static_cast<uint32_t>(Region_.Y())},
                                              .Patch = std::move(patch),
                                              .Write = {}});
  Start([store = std::move(store)](const std::stop_token &stop) { return store->Advance(stop); });
  return Result{};
}
}
