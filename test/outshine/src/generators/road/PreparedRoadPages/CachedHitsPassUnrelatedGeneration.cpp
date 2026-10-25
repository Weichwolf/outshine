#include "PreparedRoadPageFixture.h"
#include "Earth.h"
#include "ControlledRoadPageProducer.h"
#include <stop_token>
#include "PreparedRoadPages.h"
#include "Check.h"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <semaphore>
#include <string>
#include <system_error>
#include <thread>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Generators;
constexpr size_t kLimit = size_t{1024} * 1024;
constexpr LongitudeLatitude kAnchor{.LongitudeDeg = 8, .LatitudeDeg = 49};

RoadPageAddress Address(uint32_t part) {
  return {.PlanKey = std::string(64, 'a'),
          .MaterialKey = std::string(64, 'b'),
          .MaterialIndex = 0,
          .Anchor = kAnchor,
          .Level = 0,
          .Parts = {{.Kind = RoadPagePartKind::Interval, .Index = part}}};
}

RoadPageGeometry Triangle(uint32_t part) {
  return {.Mesh = {.PositionM = {0, 0, 0, 1, 0, 0, 0, 0, -1},
                   .NormalM = {0, 1, 0, 0, 1, 0, 0, 1, 0},
                   .ColourRgba = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
                   .Index = {0, 1, 2}},
          .Swept = {},
          .Diagnostics = {},
          .Parts = {{.Kind = RoadPagePartKind::Interval, .Index = part}}};
}

void CheckConcurrent(const std::shared_ptr<PreparedRoadPages> &pages) {
  using namespace outshine::Test;
  const auto warm = outshine::Test::PrepareAndLoadRoadPage(
      *pages,
      {.Address = Address(8), .Parent = {}, .RequestKey = {}, .NativeBytesMost = kLimit},
      outshine::Test::RoadProducer([](const RoadPageRequest &, const std::stop_token &) {
        return Triangle(8);
      }).get());
  CHECK(warm.has_value(), "an independent complete native page is prepared");
  if (!warm) { return; }
  using Built = std::expected<RoadPageLease, std::string>;
  using Read = std::expected<std::optional<RoadPageLease>, std::string>;

  struct State {
    std::binary_semaphore Entered{0};
    std::binary_semaphore Release{0};
    std::binary_semaphore RivalStarted{0};
    std::binary_semaphore WarmDone{0};
    std::atomic<size_t> Factories{0};
    std::optional<Built> First;
    std::optional<Built> Rival;
    std::optional<Read> Warm;
  };

  const auto state = std::make_shared<State>();
  std::jthread first([state, pages] {
    state->First = outshine::Test::PrepareAndLoadRoadPage(
        *pages,
        {.Address = Address(7), .Parent = {}, .RequestKey = {}, .NativeBytesMost = kLimit},
        outshine::Test::RoadProducer([state](const RoadPageRequest &, const std::stop_token &) {
          ++state->Factories;
          state->Entered.release();
          state->Release.acquire();
          return Triangle(7);
        }).get());
  });
  const bool entered = state->Entered.try_acquire_for(std::chrono::seconds(5));
  CHECK(entered, "the slow producer has started without publishing a partial page");
  if (!entered) {
    state->Release.release();
    return;
  }
  std::jthread rival([state, pages] {
    state->RivalStarted.release();
    state->Rival = outshine::Test::PrepareAndLoadRoadPage(
        *pages,
        {.Address = Address(7), .Parent = {}, .RequestKey = {}, .NativeBytesMost = kLimit},
        outshine::Test::RoadProducer([state](const RoadPageRequest &, const std::stop_token &) {
          ++state->Factories;
          return Triangle(7);
        }).get());
  });
  const bool competing = state->RivalStarted.try_acquire_for(std::chrono::seconds(5));
  std::jthread hit([state, pages, key = warm->Record.Key] {
    state->Warm = pages->LoadKey(key, kLimit);
    state->WarmDone.release();
  });
  const bool available = state->WarmDone.try_acquire_for(std::chrono::seconds(5));
  CHECK(available, "an unrelated cached hit completes while the producer is still blocked");
  state->Release.release();
  first.join();
  rival.join();
  hit.join();
  CHECK(competing && state->First && *state->First && state->Rival && *state->Rival &&
            state->First->value().Record.Key == state->Rival->value().Record.Key &&
            state->Factories.load() == 1,
        "concurrent requests for one missing key still share exactly one successful producer");
  CHECK(state->Warm && state->Warm->has_value() && state->Warm->value().has_value() &&
            state->Warm->value()->Record.Key == warm->Record.Key,
        "the independent page remains complete and unchanged");
}

void CheckRetry(PreparedRoadPages &pages) {
  using namespace outshine::Test;
  size_t attempts = 0;
  const auto failed = outshine::Test::PrepareAndLoadRoadPage(
      pages,
      {.Address = Address(9), .Parent = {}, .RequestKey = {}, .NativeBytesMost = kLimit},
      outshine::Test::RoadProducer([&](const RoadPageRequest &, const std::stop_token &)
                                       -> std::expected<RoadPageGeometry, std::string> {
        ++attempts;
        return std::unexpected("controlled producer failure");
      }).get());
  CHECK(!failed, "a failed build does not publish a complete page");
  const auto retry = outshine::Test::PrepareAndLoadRoadPage(
      pages,
      {.Address = Address(9), .Parent = {}, .RequestKey = {}, .NativeBytesMost = kLimit},
      outshine::Test::RoadProducer([&](const RoadPageRequest &, const std::stop_token &) {
        ++attempts;
        return Triangle(9);
      }).get());
  CHECK(retry && attempts == 2, "a later demand can retry after the failed producer is released");
}
}

int main() {
  using namespace outshine::Test;
  const auto root = std::filesystem::temp_directory_path() /
                    ("outshine-road-page-concurrent-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(root);
  auto opened = PreparedRoadPages::Open(root.string());
  CHECK(opened.has_value(), "the existing native cache opens");
  if (!opened) { return Report(); }
  auto pages = std::shared_ptr<PreparedRoadPages>(std::move(*opened));
  CheckConcurrent(pages);
  CheckRetry(*pages);
  pages.reset();
  std::error_code error;
  std::filesystem::remove_all(root, error);
  CHECK(!error, "the concurrent cache fixture is cleaned after all threads join");
  return Report();
}
