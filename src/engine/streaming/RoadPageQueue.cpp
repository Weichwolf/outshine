#include "RoadPageQueueWork.h"
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
RoadPageQueue::RoadPageQueue(Tasks &pool,
                             std::shared_ptr<Generators::PreparedRoadPages> pages,
                             Limits limits,
                             std::shared_ptr<AssetPipeline> publications)
    : Pool_(pool),
      Pages_(std::move(pages)),
      Limits_(limits),
      Publications_(std::move(publications)) {
  if (Publications_) {
    auto consumer = Publications_->OpenConsumer({.Identity = Pages_});
    if (consumer) { Consumer_.emplace(std::move(*consumer)); }
  }
}

RoadPageQueue::~RoadPageQueue() {
  for (auto &[key, work] : Work_) {
    (void)work->Stop.request_stop();
    if (work->Ticket) { work->Ticket->Cancel(); }
    CancelBinding(work);
  }
}

bool RoadPageQueue::RequestCached(const std::string &key, Manifest dependency) {
  const auto admission = RequestState(key, Kind::Payload);
  if (admission != Admission::Available) { return admission == Admission::Pending; }
  return Enqueue(key,
                 Kind::Payload,
                 [pages = Pages_,
                  failedPage = key,
                  manifest = std::move(dependency.Key),
                  bytesMost = Limits_.PageBytesMost](const std::stop_token &) -> Result {
                   auto loaded = pages->LoadKey(failedPage, bytesMost);
                   if (loaded && !*loaded && !manifest.empty()) {
                     const auto invalidated =
                         pages->InvalidateManifest(manifest, failedPage, bytesMost);
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
                   return std::optional<Product>(
                       std::make_shared<const AssetRecord>(std::move(**record)));
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
  if (found != Work_.end()) {
    (void)found->second->Stop.request_stop();
    if (found->second->Ticket) { found->second->Ticket->Cancel(); }
    CancelBinding(found->second);
  }
}

bool RoadPageQueue::SetGeneration(AssetConsumerGeneration generation) noexcept {
  if (!Consumer_) { return false; }
  auto &consumer = *Consumer_;
  if (generation <= consumer.Generation()) { return false; }
  for (auto &[key, work] : Work_) {
    (void)work->Stop.request_stop();
    CancelBinding(work);
  }
  return consumer.Advance(generation);
}

std::vector<RoadPageQueue::Completed> RoadPageQueue::Collect() {
  std::vector<Completed> completed;
  for (auto it = Work_.begin(); it != Work_.end();) {
    const auto &work = it->second;
    if (work->Type == Kind::Publication && !work->Complete.load(std::memory_order_acquire)) {
      PollPublication(work);
    }
    if (!work->Complete.load(std::memory_order_acquire)) {
      ++it;
      continue;
    }
    if (work->Type == Kind::Publication && work->Bound) { CompleteBinding(work); }
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
