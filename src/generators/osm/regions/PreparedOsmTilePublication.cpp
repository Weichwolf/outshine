#include "PreparedOsmTiles.h"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <ratio>
#include <utility>

namespace outshine::Generators::Osm {
void PreparedOsmTiles::SetPublication(Work &work, std::optional<PreparedAssetWrite> publication) {
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

void PreparedOsmTiles::Defer(Work &work) {
  const auto exponent = std::min(work.Retries, 4U);
  work.Retries = std::min(work.Retries + 1U, 4U);
  work.RetryAt =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(int64_t{1} << exponent);
  ++PublicationDeferred_;
}

void PreparedOsmTiles::Lookup(const std::string &request, Work &work) {
  if (work.Cancelled.load()) {
    work.Result = std::unexpected("native OSM preparation cancelled");
    return;
  }
  const auto began = std::chrono::steady_clock::now();
  auto loaded = Load(request, work.Definition, ReadCause::Lookup);
  ReadMs_.fetch_add(
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count());
  if (!loaded) {
    if (loaded.error() == AssetCacheError::Busy) {
      Defer(work);
    } else {
      work.Result = std::unexpected("native OSM tile cache failed");
    }
    return;
  }
  work.Result = std::move(*loaded);
  work.Retries = 0;
  work.Stage = *work.Result ? Work::Phase::Ready : Work::Phase::Source;
}

void PreparedOsmTiles::Publish(const std::string &request, Work &work) {
  if (work.Cancelled.load()) {
    SetPublication(work, std::nullopt);
    work.Result = std::unexpected("native OSM preparation cancelled");
    return;
  }
  auto loaded = Load(request, work.Definition, ReadCause::Publication);
  if (!loaded) {
    if (loaded.error() == AssetCacheError::Busy) {
      Defer(work);
    } else {
      SetPublication(work, std::nullopt);
      work.Result = std::unexpected("native OSM tile cache failed");
    }
    return;
  }
  if (*loaded) {
    SetPublication(work, std::nullopt);
    work.Result = std::move(*loaded);
    work.Stage = Work::Phase::Ready;
    return;
  }
  if (work.Stage == Work::Phase::Restore) {
    work.Result = std::unexpected("published native OSM tile is unavailable");
    return;
  }
  if (!work.Publication) {
    work.Result = std::unexpected("native OSM publication has no prepared owner");
    return;
  }
  ++PublicationAttempts_;
  const auto published = Cache_->Publish(*work.Publication);
  if (!published) {
    if (published.error() == AssetCacheError::Busy) {
      Defer(work);
    } else {
      SetPublication(work, std::nullopt);
      work.Result = std::unexpected("native OSM publication failed");
    }
    return;
  }
  ++Writes_;
  SetPublication(work, std::nullopt);
  work.Stage = Work::Phase::Restore;
  work.Retries = 0;
  loaded = Load(request, work.Definition, ReadCause::Publication);
  if (!loaded) {
    if (loaded.error() == AssetCacheError::Busy) {
      Defer(work);
    } else {
      work.Result = std::unexpected("published native OSM tile cache failed");
    }
    return;
  }
  if (!*loaded) {
    work.Result = std::unexpected("published native OSM tile is unavailable");
    return;
  }
  work.Result = std::move(*loaded);
  work.Stage = Work::Phase::Ready;
}
}
