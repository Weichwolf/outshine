#include "PreparedImpostorAssets.h"
#include "Sha256.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <ratio>
#include <span>
#include <string_view>
#include <system_error>
#include <utility>

namespace outshine::Content {
PreparedImpostorAssets::PreparedImpostorAssets(Tasks &tasks, const Config &config)
    : Tasks_(&tasks),
      Directory_(config.Directory),
      MostPending_(config.Pending),
      MostBytes_(config.ReadBytes) {}

PreparedImpostorAssets::~PreparedImpostorAssets() {
  for (const auto &pending : Pending_) { Tasks_->Wait(pending.Job); }
}

bool PreparedImpostorAssets::Opens(std::string &error) {
  if (Cache_) { return true; }
  if (!Enabled()) {
    error = "native impostor storage is disabled";
    return false;
  }
  std::error_code failure;
  std::filesystem::create_directories(Directory_, failure);
  if (failure) {
    error = "native impostor directory could not be created";
    return false;
  }
  auto opened =
      AssetCache::Open((std::filesystem::path(Directory_) / "prototypes.sqlite").string());
  if (!opened) {
    error = "native impostor database could not be opened";
    return false;
  }
  Cache_ = std::move(*opened);
  return true;
}

bool PreparedImpostorAssets::Publish(const ImpostorAtlas &atlas,
                                     std::string_view provenance,
                                     std::string &error) {
  const auto began = std::chrono::steady_clock::now();
  auto bytes = atlas.Encode(provenance, error);
  if (!bytes) { return false; }
  if (MostBytes_ == 0 || bytes->size() > MostBytes_) {
    error = "crown artifact exceeds the configured read budget";
    return false;
  }
  Box bounds{.Min = atlas.CentreM(), .Max = atlas.CentreM()};
  for (size_t axis = 0; axis < 3; ++axis) {
    bounds.Min[axis] -= atlas.HalfExtentM();
    bounds.Max[axis] += atlas.HalfExtentM();
  }
  const AssetRecord record{.Key = Sha256Hex(provenance),
                           .Kind = "impostor-prototype",
                           .Bounds = bounds,
                           .Package = {},
                           .ByteCount = bytes->size(),
                           .Parent = {}};
  const std::scoped_lock lock(Lock_);
  if (!Opens(error)) { return false; }
  if (!Cache_->Publish(std::span(&record, 1), *bytes)) {
    error = "crown artifact publication failed";
    return false;
  }
  ++Writes_;
  WriteMs_ +=
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  return true;
}

PreparedImpostorAssets::Request PreparedImpostorAssets::Read(std::string provenance) {
  if (std::ranges::any_of(Pending_, [&](const Pending &pending) {
        return pending.Result->Provenance == provenance;
      })) {
    return Request::Existing;
  }
  if (Pending_.size() >= MostPending_) { return Request::Full; }
  auto result = std::make_shared<Loaded>();
  result->Provenance = std::move(provenance);
  const auto job = Tasks_->Post([this, result] {
    const auto began = std::chrono::steady_clock::now();
    std::optional<CachedAsset> asset;
    {
      const std::scoped_lock lock(Lock_);
      if (Opens(result->Error) && MostBytes_ > 0) {
        auto loaded = Cache_->Load(Sha256Hex(result->Provenance), MostBytes_);
        if (loaded) {
          asset = std::move(*loaded);
        } else {
          result->Error = "native impostor package could not be loaded within its budget";
        }
      }
    }
    if (asset) {
      ReadBytes_ += asset->Bytes().size();
      result->Atlas = ImpostorAtlas::Decode(asset->Bytes(), result->Provenance, result->Error);
    }
    if (result->Atlas) {
      ++Hits_;
    } else {
      ++Misses_;
      if (result->Error.empty()) { result->Error = "native impostor is absent or unreadable"; }
    }
    ReadMs_ +=
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  });
  Pending_.push_back({.Result = std::move(result), .Job = job});
  return Request::Queued;
}

std::optional<PreparedImpostorAssets::Loaded> PreparedImpostorAssets::Take() {
  if (Pending_.empty() || !Tasks_->TakeCompletion(Pending_.front().Job)) { return std::nullopt; }
  Loaded result = std::move(*Pending_.front().Result);
  Pending_.pop_front();
  return result;
}

bool PreparedImpostorAssets::AwaitProgress(double seconds) {
  return !Pending_.empty() && Tasks_->AwaitCompletion(Pending_.front().Job, seconds);
}

PreparedImpostorAssets::Counters PreparedImpostorAssets::Costs() const noexcept {
  return {.Hits = Hits_,
          .Misses = Misses_,
          .Writes = Writes_,
          .ReadBytes = ReadBytes_,
          .ReadMs = ReadMs_,
          .WriteMs = WriteMs_};
}
}
