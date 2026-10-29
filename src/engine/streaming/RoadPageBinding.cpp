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
void RoadPageQueue::Work::FinishBinding() {
  const bool complete = Bound.has_value();
  if (complete) { Binding.reset(); }
  auto expected = BindingStatus::Running;
  if (BindingState.compare_exchange_strong(expected,
                                           complete ? BindingStatus::Complete
                                                    : BindingStatus::Deferred,
                                           std::memory_order_acq_rel,
                                           std::memory_order_acquire)) {
    if (complete) { Complete.store(true, std::memory_order_release); }
    return;
  }
  Bound.emplace(std::unexpected("road page request canceled"));
  Binding.reset();
  BindingState.store(BindingStatus::Complete, std::memory_order_release);
  Complete.store(true, std::memory_order_release);
}

void RoadPageQueue::RunBinding(const std::shared_ptr<Work> &work,
                               const std::shared_ptr<Generators::PreparedRoadPages> &pages) {
  using Pages = Generators::PreparedRoadPages;
  const auto stop = work->Stop.get_token();
  if (stop.stop_requested()) {
    work->Bound.emplace(std::unexpected("road page request canceled"));
  } else if (!work->Request) {
    work->Bound.emplace(std::unexpected("road alias lost its request"));
  } else {
    if (!work->Binding) {
      auto prepared = Pages::PreparePage(*work->Request);
      if (!prepared) {
        work->Bound.emplace(std::unexpected(Pages::DescribeFailure(prepared.error())));
        work->FinishBinding();
        return;
      }
      work->Binding.emplace(std::move(*prepared));
    }
    auto bound = pages->Prepare(*work->Binding, nullptr, stop);
    if (bound) {
      work->Bound.emplace(std::move(*bound));
    } else {
      const auto *cache = std::get_if<AssetCacheError>(&bound.error());
      if (cache != nullptr && *cache == AssetCacheError::Busy && !stop.stop_requested()) {
        work->RetryAt = std::chrono::steady_clock::now() + work->Delay;
        work->Delay = std::min(work->Delay * 2, std::chrono::milliseconds(16));
      } else {
        work->Bound.emplace(std::unexpected(Pages::DescribeFailure(bound.error())));
      }
    }
  }
  work->FinishBinding();
}

void RoadPageQueue::PostBinding(const std::shared_ptr<Work> &work) {
  auto expected = Work::BindingStatus::Deferred;
  if (!work->BindingState.compare_exchange_strong(expected, Work::BindingStatus::Running)) {
    return;
  }
  if (Pool_.PostDetached([work, pages = Pages_] { RunBinding(work, pages); })) { return; }
  work->Bound.emplace(std::unexpected("road page alias dispatcher stopped"));
  work->FinishBinding();
}

void RoadPageQueue::CancelBinding(const std::shared_ptr<Work> &work) {
  if (!work->BindingQueued) { return; }
  auto state = work->BindingState.load(std::memory_order_acquire);
  while (state == Work::BindingStatus::Running || state == Work::BindingStatus::Deferred) {
    if (!work->BindingState.compare_exchange_weak(state,
                                                  Work::BindingStatus::CancelRequested,
                                                  std::memory_order_acq_rel,
                                                  std::memory_order_acquire)) {
      continue;
    }
    if (state == Work::BindingStatus::Deferred &&
        !Pool_.PostDetached([work, pages = Pages_] { RunBinding(work, pages); })) {
      work->Bound.emplace(std::unexpected("road page alias dispatcher stopped"));
      work->FinishBinding();
    }
    return;
  }
}
}
