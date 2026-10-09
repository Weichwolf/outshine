#include "GroundPatchPreparation.h"
#include <atomic>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <utility>

namespace outshine {
struct GroundPatchPreparation::Work {
  Result Value;
  std::atomic<bool> Complete{false};
};

GroundPatchPreparation::GroundPatchPreparation(
    Tasks &pool,
    std::shared_ptr<Generators::PreparedTerrainAssets> assets,
    std::string key,
    Generators::Tile region,
    int side)
    : Pool_(pool), Assets_(std::move(assets)), Key_(std::move(key)), Region_(region), Side_(side) {
  Post([assets = Assets_, key = Key_, region, side] {
    return assets->LoadPatch(key,
                             {.Zoom = region.Zoom(),
                              .X = static_cast<uint32_t>(region.X()),
                              .Y = static_cast<uint32_t>(region.Y())},
                             side);
  });
}

GroundPatchPreparation::~GroundPatchPreparation() {
  Stop_.request_stop();
}

void GroundPatchPreparation::Post(std::function<Result()> operation) {
  Work_ = std::make_shared<Work>();
  if (Pool_.PostDetached(
          [work = Work_, stop = Stop_.get_token(), operation = std::move(operation)] {
            work->Value = stop.stop_requested()
                              ? Result{std::unexpected("terrain sampling patch canceled")}
                              : operation();
            work->Complete.store(true, std::memory_order_release);
          })) {
    return;
  }
  Work_->Value = std::unexpected("terrain sampling patch compute queue is closed");
  Work_->Complete.store(true, std::memory_order_release);
}

GroundPatchPreparation::Result GroundPatchPreparation::Advance(const GroundQuery &heights,
                                                               Generators::Snapped *how) {
  using Generators::Snapped;
  *how = Snapped::Waiting;
  if (Phase_ == Phase::Reading || Phase_ == Phase::Storing) {
    if (!Work_->Complete.load(std::memory_order_acquire)) { return Result{}; }
    Result_ = std::move(Work_->Value);
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
  Post([assets = Assets_, key = Key_, region = Region_, side = Side_, patch = std::move(patch)]
       -> Result {
    const Data::TileId at{.Zoom = region.Zoom(),
                          .X = static_cast<uint32_t>(region.X()),
                          .Y = static_cast<uint32_t>(region.Y())};
    auto stored = assets->StorePatch(key, at, *patch);
    if (!stored) { return std::unexpected(std::move(stored.error())); }
    auto loaded = assets->LoadPatch(key, at, side);
    if (loaded && !*loaded) { return std::unexpected("stored terrain sampling patch is absent"); }
    return loaded;
  });
  return Result{};
}
}
