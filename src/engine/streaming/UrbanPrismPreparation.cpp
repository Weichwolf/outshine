#include "UrbanPrismPreparation.h"
#include "AssetCache.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <expected>
#include <memory>
#include <mutex>
#include <optional>
#include <ratio>
#include <stop_token>
#include <string>
#include <utility>

namespace outshine {
struct UrbanPrismPreparation::Work {
  Result Value;
  Costs Measured;
  std::atomic<bool> Complete{false};
  std::mutex Mutex;
  std::condition_variable Landed;

  void Finish() {
    {
      const std::scoped_lock lock(Mutex);
      Complete.store(true, std::memory_order_release);
    }
    Landed.notify_all();
  }
};

namespace {
using Preparation = UrbanPrismPreparation;

Preparation::Result
DecodeProduct(const CachedAsset &loaded, size_t bytesMost, Preparation::Costs &costs) {
  if (loaded.Record().Kind != "urban-prisms") {
    return std::unexpected("ready prism demand refers to another asset kind");
  }
  costs.ReadBytes += loaded.Bytes().size();
  auto decoded = Generators::Osm::DecodeUrbanPrisms(loaded.Bytes(), bytesMost);
  if (!decoded) { return std::unexpected(std::move(decoded.error())); }
  return Preparation::Product(std::make_shared<Generators::Osm::UrbanPrisms>(std::move(*decoded)));
}

Preparation::Result ReadProduct(const AssetCache &cache,
                                const std::string &request,
                                size_t bytesMost,
                                Preparation::Costs &costs) {
  const auto record = cache.FindRequest(request);
  if (!record) { return std::unexpected("could not look up ready prism asset"); }
  if (!*record) { return Preparation::Product{}; }
  auto loaded = cache.Load((**record).Key, bytesMost);
  if (!loaded) { return std::unexpected("could not read ready prism asset"); }
  if (!*loaded) { return Preparation::Product{}; }
  return DecodeProduct(**loaded, bytesMost, costs);
}

Preparation::Result BuildProduct(AssetCache &cache,
                                 const std::string &request,
                                 size_t bytesMost,
                                 const AssetFactory &factory,
                                 std::stop_token stop,
                                 Preparation::Costs &costs) {
  const auto bounded = [&](size_t budget) -> std::expected<GeneratedAssetPackage, std::string> {
    if (stop.stop_requested()) { return std::unexpected("prism preparation canceled"); }
    ++costs.Generations;
    auto package = factory(budget);
    if (stop.stop_requested()) { return std::unexpected("prism preparation canceled"); }
    return package;
  };
  auto stored = ResolveAssetRequest(cache, request, bytesMost, bounded);
  if (!stored) { return std::unexpected(std::move(stored.error())); }
  return DecodeProduct(*stored, bytesMost, costs);
}
}

UrbanPrismPreparation::UrbanPrismPreparation(Tasks &pool,
                                             std::string database,
                                             std::string request,
                                             size_t bytesMost)
    : Pool_(pool),
      Database_(std::move(database)),
      Request_(std::move(request)),
      BytesMost_(bytesMost) {
  if (Database_.empty() || Request_.size() != 64 ||
      Request_.find_first_not_of("0123456789abcdef") != std::string::npos || BytesMost_ == 0 ||
      BytesMost_ > Generators::Osm::kUrbanPrismBytesMost) {
    Result_ = std::unexpected("invalid prism preparation demand or byte budget");
    Phase_ = Phase::Failed;
    return;
  }
  Post(Operation::Read);
}

bool UrbanPrismPreparation::InFlight() const noexcept {
  return Work_ && !Work_->Complete.load(std::memory_order_acquire);
}

bool UrbanPrismPreparation::AwaitProgress(double seconds) const {
  if (!Work_) { return true; }
  if (!(seconds > 0.0)) { return Work_->Complete.load(std::memory_order_acquire); }
  std::unique_lock lock(Work_->Mutex);
  return Work_->Landed.wait_for(lock, std::chrono::duration<double>(seconds), [this] {
    return Work_->Complete.load(std::memory_order_acquire);
  });
}

UrbanPrismPreparation::~UrbanPrismPreparation() {
  Cancel();
}

void UrbanPrismPreparation::Post(Operation operation, AssetFactory factory) {
  Work_ = std::make_shared<Work>();
  auto run = [work = Work_,
              stop = Stop_.get_token(),
              database = Database_,
              request = Request_,
              bytesMost = BytesMost_,
              operation,
              factory = std::move(factory)] mutable {
    const auto began = std::chrono::steady_clock::now();
    if (stop.stop_requested()) {
      work->Value = std::unexpected("prism preparation canceled");
    } else {
      auto cache = AssetCache::Open(database);
      if (!cache) {
        work->Value = std::unexpected("could not open ready prism cache");
      } else {
        work->Value =
            operation == Operation::Read
                ? ReadProduct(**cache, request, bytesMost, work->Measured)
                : BuildProduct(**cache, request, bytesMost, factory, stop, work->Measured);
      }
    }
    factory = {};
    const double elapsed =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
    if (operation == Operation::Read) {
      work->Measured.ReadWorkerMs = elapsed;
    } else {
      work->Measured.BuildWorkerMs = elapsed;
    }
    work->Finish();
  };
  if (Pool_.PostDetached(std::move(run))) { return; }
  Work_->Value = std::unexpected("prism preparation compute queue is closed");
  Work_->Finish();
}

UrbanPrismPreparation::Result UrbanPrismPreparation::Advance(const Capture &capture) {
  if (Stop_.stop_requested()) {
    Phase_ = Phase::Failed;
    Result_ = std::unexpected("prism preparation canceled");
  }
  if (Phase_ == Phase::Reading || Phase_ == Phase::Building) {
    if (!Work_->Complete.load(std::memory_order_acquire)) { return Product{}; }
    Result_ = std::move(Work_->Value);
    Costs_.ReadWorkerMs += Work_->Measured.ReadWorkerMs;
    Costs_.BuildWorkerMs += Work_->Measured.BuildWorkerMs;
    Costs_.ReadBytes += Work_->Measured.ReadBytes;
    Costs_.Generations += Work_->Measured.Generations;
    Work_.reset();
    if (!Result_) {
      Phase_ = Phase::Failed;
    } else {
      Phase_ = *Result_ ? Phase::Ready : Phase::Inputs;
    }
  }
  if (Phase_ == Phase::Ready || Phase_ == Phase::Failed) { return Result_; }
  if (!capture) { return Product{}; }
  const auto began = std::chrono::steady_clock::now();
  auto captured = capture(Stop_.get_token());
  Costs_.CaptureMainMs +=
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  if (!captured) { return Product{}; }
  if (!*captured || !(**captured)) {
    Phase_ = Phase::Failed;
    Result_ = std::unexpected(*captured ? "captured prism factory is empty" : captured->error());
    return Result_;
  }
  Phase_ = Phase::Building;
  Post(Operation::Build, std::move(**captured));
  return Product{};
}
}
