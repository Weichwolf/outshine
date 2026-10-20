#include "GroundRegionPreparation.h"
#include "PreparedRoadPages.h"
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

namespace {
std::expected<void, std::string>
RestoreSurfacePages(GroundRegionPreparation::Completed &result,
                    const std::shared_ptr<Generators::PreparedRoadPages> &pages) {
  if (!result.Loaded || result.Loaded->Region.SurfaceManifest.empty()) { return {}; }
  if (!pages) { return std::unexpected("ground region surface products have no cache service"); }
  auto manifest = pages->LoadManifest(result.Loaded->Region.SurfaceManifest);
  if (!manifest) { return std::unexpected(std::move(manifest.error())); }
  if (!*manifest) {
    result.Loaded.reset();
    result.SurfacePages.reset();
  } else {
    result.SurfacePages = std::move(**manifest);
  }
  return {};
}
}

GroundRegionPreparation::GroundRegionPreparation(
    Tasks &pool, Factory factory, std::shared_ptr<Generators::PreparedRoadPages> pages)
    : Work_(std::make_shared<Work>()) {
  const auto run = [work = Work_,
                    stop = Stop_.get_token(),
                    factory = std::move(factory),
                    pages = std::move(pages)] {
    const auto began = std::chrono::steady_clock::now();
    if (stop.stop_requested()) {
      work->Result.emplace(std::unexpected("ground region preparation canceled"));
    } else {
      work->Result = factory(stop);
      if (*work->Result) {
        const auto restored = RestoreSurfacePages(work->Result->value(), pages);
        if (!restored) { work->Result.emplace(std::unexpected(restored.error())); }
      }
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

const std::expected<GroundRegionPreparation::Completed, std::string> *
GroundRegionPreparation::Peek() const noexcept {
  if (!Work_->Complete.load(std::memory_order_acquire) || !Work_->Result) { return nullptr; }
  return &*Work_->Result;
}
}
