#include "PreparedUrbanWorker.h"
#include <atomic>
#include <chrono>
#include <expected>
#include <string>
#include <utility>
#include <variant>

namespace outshine::Generators::Osm {
std::expected<PreparedUrbanAreas::Loaded, std::string>
PreparedUrbanAreas::Worker::ReadFailure(const Failure &error) {
  if (const auto *message = std::get_if<std::string>(&error)) { return std::unexpected(*message); }
  if (std::get<AssetCacheError>(error) == AssetCacheError::Busy) {
    return Loaded{.Stage = Loaded::Phase::Deferred, .Area = {}, .Key = {}};
  }
  return std::unexpected("could not load native urban-area asset");
}

std::expected<PreparedUrbanAreas::Loaded, std::string>
PreparedUrbanAreas::Worker::Publish(const std::string &request, Work &work) {
  if (Stop.stop_requested()) { return std::unexpected("urban-area preparation cancelled"); }
  if (work.Stage == Work::Phase::Publish) {
    auto existing = Read(request, work.Input);
    if (!existing) {
      if (const auto *error = std::get_if<AssetCacheError>(&existing.error());
          error != nullptr && *error == AssetCacheError::Busy) {
        ++PublicationDeferred;
      }
      return ReadFailure(existing.error());
    }
    if (existing->Area) {
      ++Hits;
      return std::move(*existing);
    }
    if (!work.Publication) { return std::unexpected("native urban-area publication is missing"); }
    ++PublicationAttempts;
    const auto committed = Cache->Publish(*work.Publication);
    if (!committed) {
      if (committed.error() == AssetCacheError::Busy) {
        ++PublicationDeferred;
        return ReadFailure(committed.error());
      }
      return std::unexpected("native urban-area publication failed");
    }
    ++Writes;
    work.Stage = Work::Phase::Restore;
    work.RetryDelay = std::chrono::milliseconds(1);
  }
  auto restored = Read(request, work.Input);
  if (!restored) {
    if (const auto *error = std::get_if<AssetCacheError>(&restored.error());
        error != nullptr && *error == AssetCacheError::Busy) {
      ++PublicationDeferred;
    }
    return ReadFailure(restored.error());
  }
  if (!restored->Area) {
    return std::unexpected("published native urban-area asset is unavailable");
  }
  return std::move(*restored);
}
}
