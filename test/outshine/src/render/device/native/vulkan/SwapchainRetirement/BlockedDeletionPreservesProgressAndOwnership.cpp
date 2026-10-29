#include "src/render/device/native/vulkan/Presenter.h"
#include "Check.h"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <latch>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>

namespace {
using namespace outshine::Render::Native;
using namespace outshine::Render::Native::Vulkan;

bool Start(PresentState &state) {
  const auto result = state.Retirement.Start();
  CHECK(result, "the bounded retirement worker starts without a native GPU");
  return result.has_value();
}

void SharedTargetsRemainWithTheirUsers() {
  auto state = std::make_shared<PresentState>();
  if (!Start(*state)) { return; }
  auto target = std::make_shared<SwapchainState>();
  const auto user = target;
  CHECK(!state->Retirement.Take(target) && target == user,
        "retirement cannot take a target still owned by an outstanding user");
  state->Retirement.Stop();
}

void UnrelatedRequestsProgress() {
  auto state = std::make_shared<PresentState>();
  state->Owner = std::make_shared<Context>();
  if (!Start(*state)) { return; }
  std::latch entered(1);
  std::latch release(1);
  state->Inactive.emplace_back(new SwapchainState, [&](auto *value) {
    entered.count_down();
    release.wait();
    delete value;
  });
  std::thread worker([&] { RunPresenter(state); });
  entered.wait();
  auto request = std::make_shared<TargetRequest>();
  {
    const std::scoped_lock lock(state->Gate);
    state->RequestedTarget = request;
    state->Wake();
  }
  const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(1);
  while (!request->Done.load() && std::chrono::steady_clock::now() < end) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  CHECK(request->Done.load() && !request->Failure,
        "the actual worker handles an independent target while obsolete destruction is blocked");
  release.count_down();
  state->Stop.store(true);
  state->Wake();
  worker.join();
  state->Retirement.Stop();
}

void OneJobRetainsItsSurfaceAndBoundsReplacement() {
  auto state = std::make_shared<PresentState>();
  state->Owner = std::make_shared<Context>();
  if (!Start(*state)) { return; }
  std::latch entered(1);
  std::latch release(1);
  auto surface = std::make_shared<SurfaceState>();
  surface->Owner = state->Owner;
  surface->WindowId = 7;
  const std::weak_ptr<SurfaceState> retained = surface;
  auto old = std::shared_ptr<SwapchainState>(new SwapchainState, [&](auto *value) {
    entered.count_down();
    release.wait();
    delete value;
  });
  old->Surface = std::move(surface);
  CHECK(state->Retirement.Take(old) && !old, "acceptance transfers the sole target owner");
  entered.wait();
  CHECK(!retained.expired() && state->Retirement.BlocksWindow(7) &&
            !state->Retirement.BlocksWindow(8),
        "the surface remains owned and only its retiring window is blocked");
  auto other = std::make_shared<SwapchainState>();
  CHECK(!state->Retirement.Take(other) && other,
        "a blocked destructor occupies the only job; refusal preserves the caller owner");
  state->Active = std::make_shared<SwapchainState>();
  const auto active = state->Active;
  for (size_t at = 0; at < state->Slots.size() + 1; ++at) {
    state->Inactive.push_back(std::make_shared<SwapchainState>());
  }
  auto request = std::make_shared<TargetRequest>();
  state->RequestedTarget = request;
  SwitchTarget(state);
  CHECK(request->Done.load() && request->Failure && request->Failure->Kind == ErrorKind::Busy &&
            state->Active == active && state->Inactive.size() == state->Slots.size() + 1,
        "actual target admission refuses excess retained targets without changing the active one");
  state->Inactive.pop_back();
  request = std::make_shared<TargetRequest>();
  state->RequestedTarget = request;
  SwitchTarget(state);
  CHECK(request->Done.load() && !request->Failure && !state->Active &&
            state->Inactive.size() == state->Slots.size() + 1,
        "detachment proceeds once its previous active target fits the bounded inactive storage");
  std::atomic<bool> stopped{false};
  std::thread closing([&] {
    state->Retirement.Stop();
    stopped.store(true);
  });
  CHECK(!stopped.load() && !retained.expired(),
        "shutdown cannot finish or drop the surface while actual deletion is blocked");
  release.count_down();
  closing.join();
  CHECK(stopped.load() && retained.expired() && !state->Retirement.BlocksWindow(7) && other,
        "shutdown joins deletion, clears window retirement and preserves a refused job");
}
}

int main() {
  SharedTargetsRemainWithTheirUsers();
  UnrelatedRequestsProgress();
  OneJobRetainsItsSurfaceAndBoundsReplacement();
  outshine::Test::Covers(
      "actual presenter request progress under blocked deletion; one retirement "
      "job, bounded inactive admission, retained surface and shutdown join; no GPU");
  return outshine::Test::Report();
}
