#include "RoadPageQueue.h"
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <expected>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <utility>
#include <vector>

namespace outshine {
struct RoadPageQueue::Work {
  Loader Load;
  Kind Type = Kind::Payload;
  std::stop_source Stop;
  RoadPageQueue::Result Result = std::unexpected("road page work has not completed");
  std::atomic<bool> Complete{false};
};

RoadPageQueue::RoadPageQueue(Tasks &pool,
                             std::shared_ptr<Generators::PreparedRoadPages> pages,
                             Limits limits)
    : Pool_(pool), Pages_(std::move(pages)), Limits_(limits) {}

RoadPageQueue::~RoadPageQueue() {
  for (const auto &[key, work] : Work_) { (void)work->Stop.request_stop(); }
}

bool RoadPageQueue::Request(Generators::RoadPageAddress address,
                            std::string parent,
                            std::string requestKey,
                            Factory factory) {
  const auto key = Generators::RoadPageKey(address);
  const auto admission = RequestState(key, Kind::Payload);
  if (admission != Admission::Available) { return admission == Admission::Pending; }
  return Enqueue(
      key,
      Kind::Payload,
      [pages = Pages_,
       address = std::move(address),
       parent = std::move(parent),
       requestKey = std::move(requestKey),
       factory = std::move(factory),
       bytesMost = Limits_.PageBytesMost](const std::stop_token &stop) -> Result {
        auto result = pages->Resolve(
            address,
            parent,
            requestKey,
            bytesMost,
            [&](size_t limit) -> std::expected<Generators::RoadPageGeometry, std::string> {
              if (stop.stop_requested()) { return std::unexpected("road page request canceled"); }
              if (!factory) { return std::unexpected("road page miss has no producer"); }
              auto generated = factory(limit, stop);
              if (stop.stop_requested()) { return std::unexpected("road page request canceled"); }
              return generated;
            });
        if (!result) { return std::unexpected(std::move(result.error())); }
        return std::optional<Product>(std::move(*result));
      });
}

bool RoadPageQueue::RequestCached(const std::string &key, Manifest dependency) {
  const auto admission = RequestState(key, Kind::Payload);
  if (admission != Admission::Available) { return admission == Admission::Pending; }
  return Enqueue(key,
                 Kind::Payload,
                 [pages = Pages_,
                  key,
                  manifest = std::move(dependency.Key),
                  bytesMost = Limits_.PageBytesMost](const std::stop_token &) -> Result {
                   auto loaded = pages->LoadKey(key, bytesMost);
                   if (loaded && !*loaded && !manifest.empty()) {
                     const auto invalidated = pages->InvalidateManifest(manifest);
                     if (!invalidated) { return std::unexpected(invalidated.error()); }
                   }
                   if (!loaded) { return std::unexpected(std::move(loaded.error())); }
                   if (!*loaded) { return std::nullopt; }
                   return std::optional<Product>(std::move(**loaded));
                 });
}

bool RoadPageQueue::RequestMetadata(Generators::RoadPageAddress address) {
  const auto key = Generators::RoadPageKey(address);
  const auto admission = RequestState(key, Kind::Metadata);
  if (admission != Admission::Available) { return admission == Admission::Pending; }
  return Enqueue(key,
                 Kind::Metadata,
                 [pages = Pages_, address = std::move(address), bytesMost = Limits_.PageBytesMost](
                     const std::stop_token &) -> Result {
                   auto record = pages->Inspect(address, bytesMost);
                   if (!record) { return std::unexpected(std::move(record.error())); }
                   if (!*record) { return std::nullopt; }
                   return std::optional<Product>(std::move(**record));
                 });
}

RoadPageQueue::Admission RoadPageQueue::RequestState(const std::string &key, Kind kind) const {
  if (!Pages_ || key.size() != 64) { return Admission::Rejected; }
  if (const auto found = Work_.find(key); found != Work_.end()) {
    return found->second->Stop.stop_requested() || found->second->Type != kind ? Admission::Rejected
                                                                               : Admission::Pending;
  }
  if (Work_.size() >= Limits_.PendingMost || !std::ranges::all_of(key, [](char value) {
        return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'f');
      })) {
    return Admission::Rejected;
  }
  return Admission::Available;
}

bool RoadPageQueue::Enqueue(const std::string &key, Kind kind, Loader load) {
  auto work = std::make_shared<Work>();
  work->Type = kind;
  work->Load = std::move(load);
  Work_.emplace(key, work);
  auto run = [work] {
    const auto stop = work->Stop.get_token();
    if (stop.stop_requested()) {
      work->Result = std::unexpected("road page request canceled");
    } else {
      work->Result = work->Load(stop);
      if (stop.stop_requested()) { work->Result = std::unexpected("road page request canceled"); }
    }
    work->Load = {};
    work->Complete.store(true, std::memory_order_release);
  };
  if (Pool_.PostDetached(std::move(run))) { return true; }
  Work_.erase(key);
  return false;
}

void RoadPageQueue::Cancel(const std::string &key) {
  const auto found = Work_.find(key);
  if (found != Work_.end()) { (void)found->second->Stop.request_stop(); }
}

std::vector<RoadPageQueue::Completed> RoadPageQueue::Collect() {
  std::vector<Completed> completed;
  for (auto it = Work_.begin(); it != Work_.end();) {
    const auto &work = it->second;
    if (!work->Complete.load(std::memory_order_acquire)) {
      ++it;
      continue;
    }
    if (work->Stop.stop_requested()) {
      completed.push_back(
          {.Key = it->first, .Result = std::unexpected("road page request canceled")});
    } else {
      completed.push_back({.Key = it->first, .Result = std::move(work->Result)});
    }
    it = Work_.erase(it);
  }
  return completed;
}
}
