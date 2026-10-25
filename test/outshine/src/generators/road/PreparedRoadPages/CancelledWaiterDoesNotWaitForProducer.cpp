#include "Check.h"
#include "ControlledRoadPageProducer.h"
#include "PreparedRoadPages.h"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <ratio>
#include <semaphore>
#include <stop_token>
#include <string>
#include <system_error>
#include <thread>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Generators;
constexpr double kAnchorLatitudeDeg = 49;
constexpr size_t kLimit = size_t{1024} * 1024;
constexpr uint32_t kWarmPart = 8;
constexpr uint32_t kWaitingPart = 7;
constexpr uint32_t kCancelledPart = 9;
constexpr auto kStartupLimit = std::chrono::seconds(5);
constexpr auto kRivalStartup = std::chrono::milliseconds(25);
constexpr auto kCancellationLimit = std::chrono::seconds(1);
using Result = std::expected<RoadPageLease, std::string>;

RoadPageRequest Request(uint32_t part) {
  return {.Address = {.PlanKey = std::string(64, 'a'),
                      .MaterialKey = std::string(64, 'b'),
                      .MaterialIndex = 0,
                      .Anchor = {.LongitudeDeg = 8, .LatitudeDeg = kAnchorLatitudeDeg},
                      .Level = 0,
                      .Parts = {{.Kind = RoadPagePartKind::Interval, .Index = part}}},
          .Parent = {},
          .RequestKey = {},
          .NativeBytesMost = kLimit};
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

void CheckCancelledHit(PreparedRoadPages &pages) {
  using namespace outshine::Test;
  size_t builds = 0;
  const auto producer = RoadProducer([&builds]([[maybe_unused]] const RoadPageRequest &request,
                                               [[maybe_unused]] const std::stop_token &stop) {
    ++builds;
    return Triangle(kWarmPart);
  });
  const auto warm = pages.Resolve(Request(kWarmPart), producer.get());
  CHECK(warm.has_value(), "a complete page is cached before the cancelled demand");
  if (!warm) { return; }
  std::stop_source stop;
  CHECK(stop.request_stop(), "the consumer cancels before its cache request");
  const auto cancelled = pages.Resolve(Request(kWarmPart), producer.get(), stop.get_token());
  CHECK(!cancelled && cancelled.error() == "road page request canceled",
        "an already cancelled consumer receives no cached product");
  const auto unchanged = pages.Resolve(Request(kWarmPart));
  CHECK(unchanged && unchanged->Record.Key == warm->Record.Key && builds == 1,
        "consumer cancellation preserves the complete shared cached product");
}

struct ControlledWork {
  std::binary_semaphore Started{0};
  std::binary_semaphore Release{0};
  std::binary_semaphore RivalStarted{0};
  std::binary_semaphore RivalDone{0};
  std::atomic<size_t> Builds{0};
  std::optional<Result> Owner;
  std::optional<Result> Rival;
};

void CheckCancelledRival(const std::shared_ptr<PreparedRoadPages> &pages, bool cancelledAtEntry) {
  using namespace outshine::Test;
  const uint32_t part = cancelledAtEntry ? kCancelledPart : kWaitingPart;
  const auto work = std::make_shared<ControlledWork>();
  const auto producer = RoadProducer([work, part]([[maybe_unused]] const RoadPageRequest &request,
                                                  [[maybe_unused]] const std::stop_token &stop) {
    ++work->Builds;
    work->Started.release();
    work->Release.acquire();
    return Triangle(part);
  });
  std::jthread owner([pages, work, producer, part] {
    work->Owner = pages->Resolve(Request(part), producer.get());
  });
  const bool started = work->Started.try_acquire_for(kStartupLimit);
  CHECK(started, "the original native producer holds its generation slot");
  if (!started) {
    work->Release.release();
    return;
  }
  std::stop_source stop;
  if (cancelledAtEntry) { (void)stop.request_stop(); }
  std::jthread rival([pages, work, producer, part, token = stop.get_token()] {
    work->RivalStarted.release();
    work->Rival = pages->Resolve(Request(part), producer.get(), token);
    work->RivalDone.release();
  });
  const bool competing = work->RivalStarted.try_acquire_for(kStartupLimit);
  if (!cancelledAtEntry) { std::this_thread::sleep_for(kRivalStartup); }
  const auto began = std::chrono::steady_clock::now();
  (void)stop.request_stop();
  const bool cancelled = work->RivalDone.try_acquire_for(kCancellationLimit);
  const auto waited =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  CHECK(competing && cancelled,
        "the cancelled rival finishes while the original producer is still blocked");
  Note(cancelledAtEntry ? "pre-cancelled rival wait" : "cancelled competing rival wait",
       waited,
       "ms");
  work->Release.release();
  owner.join();
  rival.join();
  CHECK(work->Owner && work->Owner->has_value() && work->Builds.load() == 1,
        "the other consumer retains one successful native generation");
  CHECK(work->Rival && !work->Rival->has_value() &&
            work->Rival->error() == "road page request canceled",
        "the cancelled rival reports cancellation instead of exposing another consumer's page");
  const auto hit = pages->Resolve(Request(part));
  CHECK(hit && work->Owner && work->Owner->has_value() &&
            hit->Record.Key == work->Owner->value().Record.Key,
        "the original result remains available to an independent valid consumer");
}
}

int main() {
  using namespace outshine::Test;
  auto root = (std::filesystem::temp_directory_path() / "outshine-road-cancel-XXXXXX").string();
  CHECK(mkdtemp(root.data()) != nullptr, "the cancellation fixture owns an isolated native cache");
  if (!std::filesystem::is_directory(root)) { return Report(); }
  auto opened = PreparedRoadPages::Open(root);
  CHECK(opened.has_value(), "the existing road page store opens");
  if (!opened) { return Report(); }
  auto pages = std::shared_ptr<PreparedRoadPages>(std::move(*opened));
  CheckCancelledHit(*pages);
  CheckCancelledRival(pages, true);
  CheckCancelledRival(pages, false);
  pages.reset();
  std::error_code error;
  std::filesystem::remove_all(root, error);
  CHECK(!error, "all fixture threads join before their isolated cache is removed");
  return Report();
}
