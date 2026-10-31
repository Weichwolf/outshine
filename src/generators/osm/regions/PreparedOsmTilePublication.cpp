#include "PreparedOsmTiles.h"
#include "Sha256.h"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <ratio>
#include <stop_token>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Generators::Osm {
PreparedOsmTiles::Production::~Production() {
  if (PublicationBytes != 0) { Owner.SetPublication(*this, std::nullopt); }
}

void PreparedOsmTiles::SetPublication(Production &work,
                                      std::optional<PreparedAssetWrite> publication) {
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

std::expected<AssetPipeline::LogicalStep, std::string>
PreparedOsmTiles::Produce(Production &work, const std::stop_token &stop) {
  if (stop.stop_requested()) { return std::unexpected("native OSM preparation cancelled"); }
  switch (work.Stage) {
    case Production::Phase::Generate: return PrepareContent(work);
    case Production::Phase::Publish: return Publish(work);
    case Production::Phase::RestorePlan: return PlanRestore(work);
    case Production::Phase::Restore: return Restore(work);
    default: return std::unexpected("invalid native OSM content stage");
  }
}

std::expected<AssetPipeline::LogicalStep, std::string>
PreparedOsmTiles::PrepareContent(Production &work) {
  const auto found = Cache_->Find(work.ProductKey);
  if (!found) {
    if (found.error() == AssetCacheError::Busy) { return std::optional<AssetProduct>{}; }
    return std::unexpected("native OSM content lookup failed");
  }
  if (*found) {
    work.Stage = Production::Phase::RestorePlan;
    return PlanRestore(work);
  }
  if (auto generated = Generate(work); !generated) {
    return std::unexpected(std::move(generated.error()));
  }
  work.Stage = Production::Phase::Publish;
  return AssetPipeline::InputAdmission{.Bytes = work.PublicationBytes};
}

std::expected<AssetPipeline::LogicalStep, std::string> PreparedOsmTiles::Publish(Production &work) {
  if (!work.Publication) { return std::unexpected("native OSM publication has no owner"); }
  ++PublicationAttempts_;
  const auto began = std::chrono::steady_clock::now();
  auto published = Cache_->Publish(*work.Publication);
  PublicationMs_.fetch_add(
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count());
  if (!published) {
    if (published.error() == AssetCacheError::Busy) {
      ++PublicationDeferred_;
      return std::optional<AssetProduct>{};
    }
    return std::unexpected("native OSM publication failed");
  }
  ++Writes_;
  work.Stage = Production::Phase::RestorePlan;
  return PlanRestore(work);
}

std::expected<AssetPipeline::LogicalStep, std::string>
PreparedOsmTiles::PlanRestore(Production &work) {
  const auto bytes = Cache_->PackageBytes(work.ProductKey, kPackageBytesMost);
  if (!bytes) {
    if (bytes.error() == AssetCacheError::Busy) { return std::optional<AssetProduct>{}; }
    return std::unexpected("native OSM content metadata failed");
  }
  if (!*bytes) {
    work.Stage = work.Publication ? Production::Phase::Publish : Production::Phase::Generate;
    return AssetPipeline::InputAdmission{.Bytes = work.Publication ? work.PublicationBytes
                                                                   : kColdBytesMost};
  }
  work.LoadMost = std::max(size_t{1}, **bytes);
  work.Stage = Production::Phase::Restore;
  const auto retained = work.PublicationBytes + (work.Landing ? work.Landing->Bytes.capacity() : 0);
  return AssetPipeline::InputAdmission{.Bytes = retained + 3 * work.LoadMost};
}

std::expected<AssetPipeline::LogicalStep, std::string> PreparedOsmTiles::Restore(Production &work) {
  auto loaded = Cache_->LoadValidated(
      work.ProductKey, "osm-tile", work.LoadMost, [](const CachedAsset &native) {
        return OsmField::NativeTileDecodeBytes(native.Bytes(), kHeapBytesMost).has_value();
      });
  if (!loaded) {
    if (loaded.error() == AssetCacheError::CapacityExceeded) {
      work.Stage = Production::Phase::RestorePlan;
      const auto retained =
          work.PublicationBytes + (work.Landing ? work.Landing->Bytes.capacity() : 0);
      return AssetPipeline::InputAdmission{.Bytes = retained};
    }
    if (loaded.error() == AssetCacheError::Busy) {
      ++PublicationDeferred_;
      return std::optional<AssetProduct>{};
    }
    return std::unexpected("published native OSM tile cache failed");
  }
  if (!*loaded) {
    work.Stage = work.Publication ? Production::Phase::Publish : Production::Phase::Generate;
    return AssetPipeline::InputAdmission{.Bytes = work.Publication ? work.PublicationBytes
                                                                   : kColdBytesMost};
  }
  PublicationReadBytes_ += (**loaded).Bytes().size();
  return std::optional<AssetProduct>(AssetProduct(std::move(**loaded)));
}
}
