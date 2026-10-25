#include "PreparedRoadPages.h"
#include "ControlledRoadPageProducer.h"
#include "Check.h"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <semaphore>
#include <stop_token>
#include <string>
#include <system_error>
#include <thread>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Generators;
constexpr size_t kLimit = size_t{1024} * 1024;
constexpr uint32_t kPart = 7;
constexpr LongitudeLatitude kAnchor{.LongitudeDeg = 8, .LatitudeDeg = 49};

RoadPageRequest Request() {
  return {.Address = {.PlanKey = std::string(64, 'a'),
                      .MaterialKey = std::string(64, 'b'),
                      .MaterialIndex = 0,
                      .Anchor = kAnchor,
                      .Level = 0,
                      .Parts = {{.Kind = RoadPagePartKind::Interval, .Index = kPart}}},
          .Parent = std::string(64, 'c'),
          .RequestKey = std::string(64, 'd'),
          .NativeBytesMost = kLimit};
}

RoadPageGeometry Triangle() {
  return {.Mesh = {.PositionM = {0, 0, 0, 1, 0, 0, 0, 0, -1},
                   .NormalM = {0, 1, 0, 0, 1, 0, 0, 1, 0},
                   .ColourRgba = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
                   .Index = {0, 1, 2}},
          .Swept = {},
          .Diagnostics = {},
          .Parts = {{.Kind = RoadPagePartKind::Interval, .Index = kPart}}};
}

void CheckRecords(PreparedRoadPages &pages) {
  using namespace outshine::Test;
  size_t calls = 0;
  const auto producer = RoadProducer([&calls]([[maybe_unused]] const RoadPageRequest &request,
                                              [[maybe_unused]] const std::stop_token &stop) {
    ++calls;
    return Triangle();
  });
  const auto request = Request();
  const auto first = pages.Prepare(request, producer.get());
  CHECK(first && first->Key == RoadPageKey(request.Address) && first->Package.size() == 64 &&
            first->ByteCount > 0 && first->OffsetBytes == 0 &&
            first->RequestKey == request.RequestKey,
        "the first cook returns its committed normalized road record");
  if (!first) { return; }
  auto alias = request;
  alias.RequestKey.assign(64, 'e');
  alias.Parent.assign(64, 'f');
  const auto second = pages.Prepare(alias, producer.get());
  CHECK(second && second->Key == first->Key && second->Package == first->Package &&
            second->Parent == request.Parent && second->RequestKey == alias.RequestKey &&
            calls == 1,
        "a record hit keeps the original product parent and binds the new request without cooking");
  const auto loaded = pages.LoadRequest(alias.RequestKey, kLimit);
  const auto expected = Triangle();
  CHECK(loaded && *loaded && (**loaded).Page->Geometry.Mesh.PositionM == expected.Mesh.PositionM &&
            (**loaded).Page->Geometry.Mesh.NormalM == expected.Mesh.NormalM &&
            (**loaded).Page->Geometry.Mesh.ColourRgba == expected.Mesh.ColourRgba &&
            (**loaded).Page->Geometry.Mesh.Index == expected.Mesh.Index,
        "the independent payload reader verifies the complete original geometry");
  std::stop_source stop;
  (void)stop.request_stop();
  const auto cancelled = pages.Prepare(request, producer.get(), stop.get_token());
  CHECK(!cancelled && cancelled.error() == "road page request canceled" && calls == 1,
        "an already cancelled record request neither exposes metadata nor starts a cook");
  auto small = request;
  small.NativeBytesMost = 1;
  CHECK(!pages.Prepare(small, producer.get()) && calls == 1,
        "metadata output cannot bypass the admitted complete-page byte ceiling");
}

void CheckShared(const std::shared_ptr<PreparedRoadPages> &pages) {
  using namespace outshine::Test;

  struct Work {
    std::binary_semaphore Started{0}, Release{0}, ReaderStarted{0};
    std::atomic<size_t> Calls{0};
    std::optional<std::expected<AssetRecord, std::string>> Record;
    std::optional<std::expected<RoadPageLease, std::string>> Payload;
  };

  const auto work = std::make_shared<Work>();
  auto request = Request();
  request.Address.PlanKey.assign(64, 'f');
  const auto producer = RoadProducer([work]([[maybe_unused]] const RoadPageRequest &asked,
                                            [[maybe_unused]] const std::stop_token &stop) {
    if (work->Calls.fetch_add(1) == 0) {
      work->Started.release();
      work->Release.acquire();
    }
    return Triangle();
  });
  std::jthread record(
      [pages, work, producer, request] { work->Record = pages->Prepare(request, producer.get()); });
  const bool started = work->Started.try_acquire_for(std::chrono::seconds(5));
  CHECK(started, "record production owns the real shared generation gate");
  if (!started) {
    work->Release.release();
    return;
  }
  std::jthread payload([pages, work, producer, request] {
    work->ReaderStarted.release();
    work->Payload = pages->Resolve(request, producer.get());
  });
  const bool reader = work->ReaderStarted.try_acquire_for(std::chrono::seconds(5));
  work->Release.release();
  record.join();
  payload.join();
  CHECK(reader && work->Record && *work->Record && work->Payload && *work->Payload &&
            work->Calls.load() == 1 &&
            work->Record->value().Key == work->Payload->value().Record.Key &&
            work->Record->value().Package == work->Payload->value().Record.Package,
        "record and payload consumers share one committed production and retain their distinct "
        "results");
}
}

int main() {
  using namespace outshine::Test;
  auto root = (std::filesystem::temp_directory_path() / "outshine-road-record-XXXXXX").string();
  CHECK(mkdtemp(root.data()) != nullptr, "the fixture owns an isolated road asset store");
  if (!std::filesystem::is_directory(root)) { return Report(); }
  auto opened = PreparedRoadPages::Open(root);
  CHECK(opened.has_value(), "the real native road asset store opens");
  if (!opened) { return Report(); }
  auto pages = std::shared_ptr<PreparedRoadPages>(std::move(*opened));
  CheckRecords(*pages);
  CheckShared(pages);
  pages.reset();
  std::error_code error;
  std::filesystem::remove_all(root, error);
  CHECK(!error, "all consumers and workers finish before generated data is removed");
  return Report();
}
