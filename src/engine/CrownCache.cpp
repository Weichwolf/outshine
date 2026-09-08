#include "CrownCache.h"
#include "Sha256.h"
#include <algorithm>

namespace outshine {
CrownCache::CrownCache(Tasks &tasks, const Config &config)
    : Tasks_(&tasks),
      Store_(config.Store),
      MostPending_(config.Pending),
      MostBytes_(config.ReadBytes) {}

CrownCache::~CrownCache() {
  for (const auto &pending : Pending_) { Tasks_->Wait(pending.Job); }
}

bool CrownCache::Publish(const CrownAtlas &atlas, std::string_view provenance, std::string &error) {
  auto bytes = atlas.Encode(provenance, error);
  if (!bytes) { return false; }
  if (MostBytes_ == 0 || bytes->size() > MostBytes_) {
    error = "crown artifact exceeds the configured read budget";
    return false;
  }
  if (!Store_.Keep(Sha256Hex(provenance), bytes->data(), bytes->size())) {
    error = "crown artifact publication failed";
    return false;
  }
  return true;
}

CrownCache::Request CrownCache::Read(std::string provenance) {
  if (std::ranges::any_of(Pending_, [&](const Pending &pending) {
        return pending.Result->Provenance == provenance;
      })) {
    return Request::Existing;
  }
  if (Pending_.size() >= MostPending_) { return Request::Full; }
  auto result = std::make_shared<Loaded>();
  result->Provenance = std::move(provenance);
  const auto job = Tasks_->Post([this, result] {
    const auto bytes =
        MostBytes_ == 0 ? std::nullopt : Store_.Read(Sha256Hex(result->Provenance), MostBytes_);
    if (!bytes) {
      result->Error = "crown artifact is absent, unreadable or exceeds the read budget";
      return;
    }
    result->Atlas = CrownAtlas::Decode(*bytes, result->Provenance, result->Error);
  });
  Pending_.push_back({.Result = std::move(result), .Job = job});
  return Request::Queued;
}

std::optional<CrownCache::Loaded> CrownCache::Take() {
  if (Pending_.empty() || !Tasks_->Done(Pending_.front().Job)) { return std::nullopt; }
  Loaded result = std::move(*Pending_.front().Result);
  Pending_.pop_front();
  return result;
}
} // namespace outshine
