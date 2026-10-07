#include "GroundRegionPreparation.h"
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
struct GroundRegionPreparation::Work {
  std::optional<std::expected<Completed, std::string>> Result;
  std::atomic<bool> Complete{false};
};

GroundRegionPreparation::GroundRegionPreparation(Tasks &pool, Factory factory)
    : Work_(std::make_shared<Work>()) {
  const auto run = [work = Work_, stop = Stop_.get_token(), factory = std::move(factory)] {
    const auto began = std::chrono::steady_clock::now();
    if (stop.stop_requested()) {
      work->Result.emplace(std::unexpected("ground region preparation canceled"));
    } else {
      work->Result = factory(stop);
      if (stop.stop_requested()) {
        work->Result.emplace(std::unexpected("ground region preparation canceled"));
      } else if (*work->Result) {
        work->Result->value().WorkerMs =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began)
                .count();
      }
    }
    work->Complete.store(true, std::memory_order_release);
  };
  if (pool.PostDetached(run)) { return; }
  Work_->Result.emplace(std::unexpected("ground region compute queue is closed"));
  Work_->Complete.store(true, std::memory_order_release);
}

GroundRegionPreparation::~GroundRegionPreparation() {
  Stop_.request_stop();
}

std::optional<std::expected<GroundRegionPreparation::Completed, std::string>>
GroundRegionPreparation::Collect() {
  if (!Work_->Complete.load(std::memory_order_acquire)) { return std::nullopt; }
  auto result = std::move(Work_->Result);
  Work_->Result.reset();
  return result;
}
}
