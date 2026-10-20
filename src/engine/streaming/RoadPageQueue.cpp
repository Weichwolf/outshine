#include "RoadPageQueue.h"
#include <atomic>
#include <cstddef>
#include <expected>
#include <memory>
#include <stop_token>
#include <string>
#include <utility>
#include <vector>

namespace outshine {
struct RoadPageQueue::Work {
  std::stop_source Stop;
  std::expected<Generators::PreparedRoadPages::Loaded, std::string> Result =
      std::unexpected("road page work has not completed");
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
  if (key.empty() || !Pages_) { return false; }
  if (const auto found = Work_.find(key); found != Work_.end()) {
    return !found->second->Stop.stop_requested();
  }
  if (Work_.size() >= Limits_.PendingMost) { return false; }
  auto work = std::make_shared<Work>();
  Work_.emplace(key, work);
  auto run = [work,
              pages = Pages_,
              address = std::move(address),
              parent = std::move(parent),
              requestKey = std::move(requestKey),
              factory = std::move(factory),
              bytesMost = Limits_.PageBytesMost] {
    const auto stop = work->Stop.get_token();
    if (stop.stop_requested()) {
      work->Result = std::unexpected("road page request canceled");
    } else {
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
      if (stop.stop_requested()) {
        work->Result = std::unexpected("road page request canceled");
      } else {
        work->Result = std::move(result);
      }
    }
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
