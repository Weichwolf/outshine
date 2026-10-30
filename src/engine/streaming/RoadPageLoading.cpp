#include "RoadPageQueueWork.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <expected>
#include <memory>
#include <string>
#include <utility>
#include <variant>

namespace outshine {
void RoadPageQueue::RunLoad(const std::shared_ptr<Work> &work) {
  const auto stop = work->Stop.get_token();
  auto loaded = stop.stop_requested()
                    ? LoadResult{std::unexpected(std::string("road page request canceled"))}
                    : work->Load(stop);
  if (stop.stop_requested()) {
    work->Result = std::unexpected("road page request canceled");
  } else if (loaded) {
    work->Result = std::move(*loaded);
  } else {
    const auto *cache = std::get_if<AssetCacheError>(&loaded.error());
    if (cache != nullptr && *cache == AssetCacheError::Busy) {
      work->RetryAt = std::chrono::steady_clock::now() + work->Delay;
      work->Delay = std::min(work->Delay * 2, std::chrono::milliseconds(16));
      work->LoadState.store(Work::LoadStatus::Deferred, std::memory_order_release);
      return;
    }
    work->Result = std::unexpected(Generators::PreparedRoadPages::DescribeFailure(loaded.error()));
  }
  work->Load = {};
  work->LoadState.store(Work::LoadStatus::Complete, std::memory_order_release);
  work->Complete.store(true, std::memory_order_release);
}

bool RoadPageQueue::PostLoad(const std::shared_ptr<Work> &work) {
  return Pool_.PostDetached([work] { RunLoad(work); });
}

void RoadPageQueue::PollLoad(const std::shared_ptr<Work> &work) {
  if (work->LoadState.load(std::memory_order_acquire) != Work::LoadStatus::Deferred ||
      (!work->Stop.stop_requested() && std::chrono::steady_clock::now() < work->RetryAt)) {
    return;
  }
  auto expected = Work::LoadStatus::Deferred;
  if (!work->LoadState.compare_exchange_strong(expected, Work::LoadStatus::Running)) { return; }
  if (!work->Stop.stop_requested() && PostLoad(work)) { return; }
  work->Result = std::unexpected(work->Stop.stop_requested() ? "road page request canceled"
                                                             : "road page read dispatcher stopped");
  work->Load = {};
  work->LoadState.store(Work::LoadStatus::Complete, std::memory_order_release);
  work->Complete.store(true, std::memory_order_release);
}
}
