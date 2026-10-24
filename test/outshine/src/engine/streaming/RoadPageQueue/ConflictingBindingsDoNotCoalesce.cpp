#include "RoadPageQueue.h"
#include "ControlledRoadPageProducer.h"
#include "Check.h"
#include "Earth.h"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <semaphore>
#include <stop_token>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
constexpr size_t kLimit = size_t{1024} * 1024;
constexpr LongitudeLatitude kAnchor{.LongitudeDeg = 8, .LatitudeDeg = 49};

RoadPageGeometry Triangle() {
  return {.Mesh = {.PositionM = {0, 0, 0, 1, 0, 0, 0, 0, -1},
                   .NormalM = {0, 1, 0, 0, 1, 0, 0, 1, 0},
                   .ColourRgba = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
                   .Index = {0, 1, 2}},
          .Swept = {},
          .Diagnostics = {},
          .Parts = {{.Kind = RoadPagePartKind::Interval, .Index = 7}}};
}

std::vector<RoadPageQueue::Completed> Wait(RoadPageQueue &queue) {
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (std::chrono::steady_clock::now() < until) {
    auto completed = queue.Collect();
    if (!completed.empty()) { return completed; }
    std::this_thread::yield();
  }
  return {};
}

void CheckBindings(const std::filesystem::path &directory) {
  using namespace outshine::Test;
  auto opened = PreparedRoadPages::Open(directory.string());
  CHECK(opened.has_value(), "the real native page cache opens");
  if (!opened) { return; }
  const std::shared_ptr<PreparedRoadPages> pages = std::move(*opened);
  Tasks pool(1);
  RoadPageQueue queue(pool, pages, {.PendingMost = 1, .PageBytesMost = kLimit});
  const RoadPageRequest first{
      .Address = {.PlanKey = std::string(64, 'a'),
                  .MaterialKey = std::string(64, 'b'),
                  .MaterialIndex = 0,
                  .Anchor = kAnchor,
                  .Level = 0,
                  .Parts = {{.Kind = RoadPagePartKind::Interval, .Index = 7}}},
      .Parent = std::string(64, 'c'),
      .RequestKey = std::string(64, 'd'),
      .NativeBytesMost = kLimit};

  struct Control {
    std::binary_semaphore Entered{0};
    std::binary_semaphore Release{0};
    std::atomic<size_t> Calls{0};
  };

  const auto control = std::make_shared<Control>();
  const auto producer = RoadProducer([control](const RoadPageRequest &, const std::stop_token &) {
    ++control->Calls;
    control->Entered.release();
    control->Release.acquire();
    return Triangle();
  });
  CHECK(queue.Request(first, producer), "the first complete binding is admitted");
  const bool running = control->Entered.try_acquire_for(std::chrono::seconds(5));
  CHECK(running, "the actual worker is held before publication");
  if (!running) {
    control->Release.release();
    return;
  }
  auto otherRequest = first;
  otherRequest.RequestKey.assign(64, 'e');
  auto otherParent = first;
  otherParent.Parent.assign(64, 'f');
  CHECK(!queue.Request(otherRequest, producer) && !queue.Request(otherParent, producer),
        "different durable bindings are explicitly rejected instead of being silently lost");
  CHECK(queue.Request(first, producer) && queue.RequestCached(RoadPageKey(first.Address)) &&
            queue.Pending() == 1 && control->Calls.load() == 1,
        "the identical request and an unbound content reader still share one producer");
  control->Release.release();
  auto complete = Wait(queue);
  CHECK(complete.size() == 1 && complete.front().Result && *complete.front().Result,
        "the original accepted request publishes its complete product");
  auto fresh = PreparedRoadPages::Open(directory.string());
  CHECK(fresh.has_value(), "a fresh offline cache connection opens after publication");
  if (!fresh) { return; }
  const auto bound = (*fresh)->LoadRequest(first.RequestKey, kLimit);
  const auto rejected = (*fresh)->LoadRequest(otherRequest.RequestKey, kLimit);
  CHECK(bound && *bound && (**bound).Record.Parent == first.Parent &&
            (**bound).Record.RequestKey == first.RequestKey,
        "the first durable parent/request binding survives the conflicting requests");
  CHECK(rejected && !*rejected && control->Calls.load() == 1,
        "a rejected alias neither appears in the index nor starts a second producer");
}
}

int main() {
  using namespace outshine::Test;
  const auto directory =
      std::filesystem::temp_directory_path() /
      ("outshine-roadpage-bindings-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(directory);
  CheckBindings(directory);
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "the fixture is removed after all workers and cache handles close");
  return Report();
}
