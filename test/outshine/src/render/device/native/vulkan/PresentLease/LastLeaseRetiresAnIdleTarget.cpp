#include "src/render/device/native/vulkan/Presenter.h"
#include "Check.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <latch>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>

namespace {
using namespace outshine::Render::Native::Vulkan;

void LastLeaseRetiresAnIdleTarget() {
  std::mutex gate;
  std::condition_variable changed;
  bool retired = false;
  std::latch inspected(1);
  std::latch released(1);
  auto context = std::make_shared<Context>();
  auto state = std::make_shared<PresentState>();
  state->Owner = context;
  auto target = std::shared_ptr<SwapchainState>(new SwapchainState, [&](auto *value) {
    delete value;
    {
      const std::scoped_lock lock(gate);
      retired = true;
    }
    changed.notify_one();
  });
  target->Owner = context;
  auto lease = std::make_shared<PresentLease>();
  lease->Target = target;
  lease->Dispatcher = state;
  state->Inactive.push_back(std::move(target));
  state->Inactive.emplace_back(new SwapchainState, [&](auto *value) {
    inspected.count_down();
    released.wait();
    delete value;
  });
  std::thread worker([&] { RunPresenter(state); });
  inspected.wait();
  lease.reset();
  released.count_down();
  {
    std::unique_lock lock(gate);
    CHECK(changed.wait_for(lock, std::chrono::seconds(1), [&] { return retired; }),
          "the last lease retires an already inspected target without another frame or request");
  }
  state->Stop.store(true);
  state->Wake();
  worker.join();
  state->Inactive.clear();
}

void ExpiredDispatcherDoesNotRetainTheTarget() {
  auto context = std::make_shared<Context>();
  auto target = std::make_shared<SwapchainState>();
  target->Owner = context;
  const std::weak_ptr<SwapchainState> observed = target;
  auto lease = std::make_shared<PresentLease>();
  lease->Target = std::move(target);
  lease->Dispatcher = std::make_shared<PresentState>();
  CHECK(lease->Dispatcher.expired(), "the retiring lease does not extend dispatcher lifetime");
  lease.reset();
  CHECK(observed.expired(), "retirement releases its target after dispatcher destruction");
}
}

int main() {
  LastLeaseRetiresAnIdleTarget();
  ExpiredDispatcherDoesNotRetainTheTarget();
  outshine::Test::Covers(
      "idle presentation retirement after the final external lease; expired "
      "dispatcher lifetime; actual worker cleanup ordering without native handles");
  return outshine::Test::Report();
}
