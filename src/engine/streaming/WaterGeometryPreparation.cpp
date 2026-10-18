#include "WaterGeometryPreparation.h"
#include "WaterSurfacePreparation.h"
#include <atomic>
#include <chrono>
#include <expected>
#include <memory>
#include <optional>
#include <ratio>
#include <stop_token>
#include <string>
#include <utility>

namespace outshine {
struct WaterGeometryPreparation::Work {
  std::optional<std::expected<Completed, std::string>> Result;
  std::atomic<bool> Complete{false};
};

WaterGeometryPreparation::WaterGeometryPreparation(Tasks &pool,
                                                   Generators::WaterAsset source,
                                                   TangentFrame frame)
    : Source_(std::move(source)), Work_(std::make_shared<Work>()) {
  const auto run = [work = Work_, stop = Stop_.get_token(), source = Source_, frame] {
    const auto began = std::chrono::steady_clock::now();
    auto result = Generators::PrepareWaterSurfaces(source, frame, stop);
    if (stop.stop_requested()) {
      work->Result.emplace(std::unexpected("water surface preparation canceled"));
    } else if (!result) {
      work->Result.emplace(std::unexpected(std::move(result.error())));
    } else {
      work->Result.emplace(Completed{.Water = std::move(*result),
                                     .WorkerMs = std::chrono::duration<double, std::milli>(
                                                     std::chrono::steady_clock::now() - began)
                                                     .count()});
    }
    work->Complete.store(true, std::memory_order_release);
  };
  if (pool.PostDetached(run)) { return; }
  Work_->Result.emplace(std::unexpected("water surface compute queue is closed"));
  Work_->Complete.store(true, std::memory_order_release);
}

WaterGeometryPreparation::~WaterGeometryPreparation() {
  Cancel();
}

bool WaterGeometryPreparation::Complete() const noexcept {
  return Work_->Complete.load(std::memory_order_acquire);
}

std::optional<std::expected<WaterGeometryPreparation::Completed, std::string>>
WaterGeometryPreparation::Collect() {
  if (!Complete()) { return std::nullopt; }
  auto result = std::move(Work_->Result);
  Work_->Result.reset();
  return result;
}
}
