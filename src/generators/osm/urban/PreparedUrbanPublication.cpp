#include "PreparedUrbanAreas.h"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <utility>

namespace outshine::Generators::Osm {
void PreparedUrbanAreas::SetPublication(Work &work, std::optional<PreparedAssetWrite> publication) {
  const auto bytes = publication ? publication->CapacityBytes() : 0;
  const auto previous = work.PublicationBytes;
  work.Publication = std::move(publication);
  work.PublicationBytes = bytes;
  if (bytes > previous) {
    const auto current = PublicationBytes_.fetch_add(bytes - previous) + bytes - previous;
    auto peak = PublicationPeakBytes_.load();
    while (peak < current && !PublicationPeakBytes_.compare_exchange_weak(peak, current)) {}
  } else {
    PublicationBytes_.fetch_sub(previous - bytes);
  }
}

void PreparedUrbanAreas::Defer(Work &work) {
  const auto exponent = std::min(work.Retries, 4U);
  work.Retries = std::min(work.Retries + 1U, 4U);
  work.RetryAt =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(int64_t{1} << exponent);
  ++PublicationDeferred_;
}

void PreparedUrbanAreas::Lookup(const std::string &request, Work &work) {
  if (Stop_.stop_requested() || work.Stop.stop_requested()) {
    work.Result = std::unexpected("urban-area preparation cancelled");
    return;
  }
  auto loaded = Load(request, work.Input, false);
  if (!loaded) {
    if (loaded.error() == AssetCacheError::Busy) {
      Defer(work);
    } else {
      work.Result = std::unexpected("could not restore native urban-area asset");
    }
    return;
  }
  work.Result = std::move(*loaded);
  work.Retries = 0;
  work.Stage = Work::Phase::Source;
}

void PreparedUrbanAreas::Publish(const std::string &request, Work &work) {
  if (Stop_.stop_requested() || work.Stop.stop_requested()) {
    SetPublication(work, std::nullopt);
    work.Result = std::unexpected("urban-area preparation cancelled");
    return;
  }
  auto loaded = Load(request, work.Input, true);
  if (!loaded) {
    if (loaded.error() == AssetCacheError::Busy) {
      Defer(work);
    } else {
      SetPublication(work, std::nullopt);
      work.Result = std::unexpected("could not restore native urban-area asset");
    }
    return;
  }
  if (loaded->Area) {
    SetPublication(work, std::nullopt);
    work.Result = std::move(*loaded);
    return;
  }
  if (work.Stage == Work::Phase::Restore || !work.Publication) {
    SetPublication(work, std::nullopt);
    work.Result = std::unexpected("native urban-area publication is unavailable");
    return;
  }
  ++PublicationAttempts_;
  const auto committed = Cache_->Publish(*work.Publication);
  if (!committed) {
    if (committed.error() == AssetCacheError::Busy) {
      Defer(work);
    } else {
      SetPublication(work, std::nullopt);
      work.Result = std::unexpected("native urban-area publication failed");
    }
    return;
  }
  ++Writes_;
  work.Stage = Work::Phase::Restore;
  work.Retries = 0;
  loaded = Load(request, work.Input, true);
  if (!loaded) {
    if (loaded.error() == AssetCacheError::Busy) {
      Defer(work);
    } else {
      SetPublication(work, std::nullopt);
      work.Result = std::unexpected("could not restore published native urban-area asset");
    }
    return;
  }
  if (!loaded->Area) {
    SetPublication(work, std::nullopt);
    work.Result = std::unexpected("published native urban-area asset is unavailable");
    return;
  }
  work.Result = std::move(*loaded);
  SetPublication(work, std::nullopt);
}
}
