#include "GroundRegionPreparation.h"
#include "PreparedRoadPages.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <ratio>
#include <stop_token>
#include <string>
#include <utility>
#include <variant>

namespace outshine {
struct GroundRegionPreparation::Work {
  Factory Produce;
  std::shared_ptr<Generators::PreparedRoadPages> Pages;
  std::optional<Completed> Candidate;
  std::optional<std::expected<Completed, std::string>> Result;
  enum class Status { Running, Deferred, CancelRequested, Complete };
  std::atomic<Status> State{Status::Running};
  unsigned Retries = 0;
  std::chrono::steady_clock::time_point RetryAt;
  double WorkerMs = 0;

  void Fail(const Failure &error);

  void ReleaseInputs() {
    Candidate.reset();
    Produce = {};
    Pages.reset();
  }

  void Finish() {
    if (Result) { ReleaseInputs(); }
    auto expected = Status::Running;
    if (State.compare_exchange_strong(expected,
                                      Result ? Status::Complete : Status::Deferred,
                                      std::memory_order_acq_rel,
                                      std::memory_order_acquire)) {
      return;
    }
    Result.emplace(std::unexpected("ground region preparation canceled"));
    ReleaseInputs();
    State.store(Status::Complete, std::memory_order_release);
  }
};

namespace {
using Failure = GroundRegionPreparation::Failure;

std::string Describe(const Failure &failure) {
  if (const auto *message = std::get_if<std::string>(&failure)) { return *message; }
  switch (std::get<AssetCacheError>(failure)) {
    case AssetCacheError::InvalidInput: return "invalid ground asset cache input";
    case AssetCacheError::Storage: return "ground asset cache storage failed";
    case AssetCacheError::Busy: return "ground asset cache publication is busy";
    case AssetCacheError::UnsupportedVersion: return "unsupported ground asset cache version";
    case AssetCacheError::CapacityExceeded: return "ground asset package exceeds its byte budget";
  }
  return "unknown ground asset cache failure";
}

std::expected<void, Failure>
RestoreSurfacePages(GroundRegionPreparation::Completed &result,
                    const std::shared_ptr<Generators::PreparedRoadPages> &pages) {
  if (!result.Loaded || result.Loaded->Region.SurfaceManifest.empty()) { return {}; }
  if (!pages) {
    return std::unexpected(std::string("ground region surface products have no cache service"));
  }
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
    : Pool_(pool), Work_(std::make_shared<Work>()) {
  Work_->Produce = std::move(factory);
  Work_->Pages = std::move(pages);
  Post();
}

void GroundRegionPreparation::Post() {
  Work_->State.store(Work::Status::Running, std::memory_order_release);
  if (Pool_.PostDetached([work = Work_, stop = Stop_.get_token()] { Run(*work, stop); })) {
    return;
  }
  Work_->Result.emplace(std::unexpected("ground region compute queue is closed"));
  Work_->Finish();
}

void GroundRegionPreparation::Work::Fail(const Failure &error) {

  const auto *cache = std::get_if<AssetCacheError>(&error);
  if (cache != nullptr && *cache == AssetCacheError::Busy) {
    const auto exponent = std::min(Retries, 4U);
    Retries = std::min(Retries + 1U, 4U);
    RetryAt = std::chrono::steady_clock::now() + std::chrono::milliseconds(uint64_t{1} << exponent);
  } else {
    Result.emplace(std::unexpected(Describe(error)));
  }
}

void GroundRegionPreparation::Run(Work &work, const std::stop_token &stop) {
  const auto began = std::chrono::steady_clock::now();

  if (stop.stop_requested()) {
    work.Result.emplace(std::unexpected("ground region preparation canceled"));
  } else {
    if (!work.Candidate) {
      if (!work.Produce) {
        work.Result.emplace(std::unexpected("ground region preparation has no factory"));
      } else {
        auto produced = work.Produce(stop);
        if (!produced) {
          work.Fail(produced.error());
        } else {
          work.Candidate = std::move(*produced);
          work.Produce = {};
        }
      }
    }
    if (work.Candidate && !work.Result) {
      const auto restored = RestoreSurfacePages(*work.Candidate, work.Pages);
      if (!restored) {
        work.Fail(restored.error());
      } else {
        work.Result.emplace(std::move(*work.Candidate));
      }
    }
  }
  if (stop.stop_requested()) {
    work.Result.emplace(std::unexpected("ground region preparation canceled"));
  }
  work.WorkerMs +=
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  if (work.Result && *work.Result) { work.Result->value().WorkerMs = work.WorkerMs; }
  work.Finish();
}

GroundRegionPreparation::~GroundRegionPreparation() {
  Cancel();
}

void GroundRegionPreparation::Cancel() noexcept {
  Stop_.request_stop();
  auto state = Work_->State.load(std::memory_order_acquire);
  if (state == Work::Status::Running &&
      Work_->State.compare_exchange_strong(state,
                                           Work::Status::CancelRequested,
                                           std::memory_order_acq_rel,
                                           std::memory_order_acquire)) {
    return;
  }
  if (state == Work::Status::Deferred) { Post(); }
}

void GroundRegionPreparation::Advance() {
  if (Work_->State.load(std::memory_order_acquire) == Work::Status::Deferred &&
      std::chrono::steady_clock::now() >= Work_->RetryAt) {
    Post();
  }
}

std::optional<std::expected<GroundRegionPreparation::Completed, std::string>>
GroundRegionPreparation::Collect() {
  Advance();
  if (Work_->State.load(std::memory_order_acquire) != Work::Status::Complete) {
    return std::nullopt;
  }
  return std::exchange(Work_->Result, std::nullopt);
}

const std::expected<GroundRegionPreparation::Completed, std::string> *
GroundRegionPreparation::Peek() const noexcept {
  if (Work_->State.load(std::memory_order_acquire) != Work::Status::Complete || !Work_->Result) {
    return nullptr;
  }
  return &*Work_->Result;
}
}
