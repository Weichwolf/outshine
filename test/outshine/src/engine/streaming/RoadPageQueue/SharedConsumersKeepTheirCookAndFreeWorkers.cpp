#include "RoadPublicationFixture.h"
#include "RoadPageQueue.h"
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
#include <variant>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
constexpr size_t kLimit = size_t{1024} * 1024;
constexpr double kProbeSeconds = 0.5;
constexpr auto kStartup = std::chrono::seconds(5);
constexpr auto kPoll = std::chrono::milliseconds(1);
constexpr uint32_t kPart = 7;
constexpr double kLatitude = 49;
enum class Scenario : uint8_t { Shared, CancelOwner, CancelSingleWorker, DifferentProduct };

RoadPageRequest Request(char plan) {
  return {.Address = {.PlanKey = std::string(64, plan),
                      .MaterialKey = std::string(64, 'b'),
                      .MaterialIndex = 0,
                      .Anchor = {.LongitudeDeg = 8, .LatitudeDeg = kLatitude},
                      .Level = 0,
                      .Parts = {{.Kind = RoadPagePartKind::Interval, .Index = kPart}}},
          .Parent = {},
          .RequestKey = {},
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

struct Controlled {
  std::binary_semaphore Started{0}, Release{0};
  std::atomic<size_t> Cooks{0};
  std::atomic<bool> CookCancelled{false}, IndependentRan{false};
};

std::shared_ptr<const RoadPageProducer> Producer(const std::shared_ptr<Controlled> &work) {
  return Test::RoadProducer(
      [work]([[maybe_unused]] const RoadPageRequest &request,
             const std::stop_token &stop) -> std::expected<RoadPageGeometry, std::string> {
        const std::stop_callback canceled(stop, [work] { work->CookCancelled.store(true); });
        if (work->Cooks.fetch_add(1) == 0) {
          work->Started.release();
          work->Release.acquire();
        }
        if (stop.stop_requested()) { return std::unexpected("controlled cook canceled"); }
        return Triangle();
      });
}

std::vector<RoadPageQueue::Completed> Wait(RoadPageQueue &queue) {
  const auto deadline = std::chrono::steady_clock::now() + kStartup;
  do {
    auto done = queue.Collect();
    if (!done.empty()) { return done; }
    std::this_thread::sleep_for(kPoll);
  } while (std::chrono::steady_clock::now() < deadline);
  return {};
}

const AssetRecord *Record(const std::vector<RoadPageQueue::Completed> &done) {
  if (done.size() != 1) { return nullptr; }
  const auto &result = done.front().Result;
  if (!result || !*result) { return nullptr; }
  const auto *receipt = std::get_if<RoadPageQueue::Receipt>(&**result);
  return receipt != nullptr && *receipt ? receipt->get() : nullptr;
}

char Plan(Scenario scenario) {
  if (scenario == Scenario::CancelOwner) { return 'c'; }
  if (scenario == Scenario::CancelSingleWorker) { return 'f'; }
  if (scenario == Scenario::DifferentProduct) { return 'd'; }
  return 'a';
}

const char *CookLabel(Scenario scenario) {
  if (scenario == Scenario::CancelOwner) { return "cancel-owner cooks"; }
  if (scenario == Scenario::CancelSingleWorker) { return "cancel-owner single-worker cooks"; }
  if (scenario == Scenario::DifferentProduct) { return "different-product cooks"; }
  return "shared cooks";
}

bool CancelsOwner(Scenario scenario) {
  return scenario == Scenario::CancelOwner || scenario == Scenario::CancelSingleWorker;
}

void CheckGeometry(PreparedRoadPages &pages, const RoadPageAddress &address) {
  using namespace outshine::Test;
  const auto loaded = pages.Load(address, kLimit);
  const auto expected = Triangle();
  CHECK(loaded && *loaded && (**loaded).Page->Geometry.Mesh.PositionM == expected.Mesh.PositionM &&
            (**loaded).Page->Geometry.Mesh.NormalM == expected.Mesh.NormalM &&
            (**loaded).Page->Geometry.Mesh.ColourRgba == expected.Mesh.ColourRgba &&
            (**loaded).Page->Geometry.Mesh.Index == expected.Mesh.Index,
        "the normal native loader verifies all of the surviving consumer's geometry");
}

void CheckCompletions(const std::vector<RoadPageQueue::Completed> &left,
                      const std::vector<RoadPageQueue::Completed> &right,
                      const RoadPageAddress &address,
                      Scenario scenario) {
  using namespace outshine::Test;
  const bool canceled = CancelsOwner(scenario);
  const auto *aRecord = Record(left);
  const auto *bRecord = Record(right);
  CHECK(left.size() == 1 && (canceled ? !left.front().Result && left.front().Result.error() ==
                                                                    "road page request canceled"
                                      : aRecord != nullptr),
        "A independently receives its own terminal completion or cancellation");
  CHECK(bRecord != nullptr && bRecord->Key == RoadPageKey(address),
        "the still current consumer B receives its complete committed product");
  if (scenario == Scenario::Shared) {
    CHECK(aRecord != nullptr && bRecord != nullptr && aRecord->Package == bRecord->Package,
          "both consumers refer to the same persisted immutable product");
  }
}

void CheckSharing(const std::shared_ptr<PreparedRoadPages> &pages, Scenario scenario) {
  using namespace outshine::Test;
  const auto work = std::make_shared<Controlled>();
  const auto producer = Producer(work);
  const bool canceled = CancelsOwner(scenario);
  const bool different = scenario == Scenario::DifferentProduct;
  const auto first = Request(Plan(scenario));
  const auto second = different ? Request('e') : first;
  Tasks pool(scenario == Scenario::CancelSingleWorker ? 1 : 2);
  const auto publications = outshine::Test::RoadPublications(pool);
  RoadPageQueue a(pool, pages, {.PendingMost = 1, .PageBytesMost = kLimit}, publications);
  RoadPageQueue b(pool, pages, {.PendingMost = 1, .PageBytesMost = kLimit}, publications);
  CHECK(outshine::Test::AdmitRoadPage(a, first, producer),
        "the first consumer admits a real publication job");
  const bool started = work->Started.try_acquire_for(kStartup);
  CHECK(started, "the first native cook holds its worker until explicitly released");
  if (!started) {
    work->Release.release();
    return;
  }
  CHECK(outshine::Test::AdmitRoadPage(b, second, producer),
        "the independent consumer admits its own demand");
  if (canceled) {
    a.Cancel(RoadPageKey(first.Address));
    CHECK(!work->CookCancelled.load(),
          "canceling A does not stop a cook still required by the admitted consumer B");
  } else {
    const auto probe = pool.Post([work] { work->IndependentRan.store(true); });
    const bool early = probe != Tasks::kNoTask && pool.AwaitCompletion(probe, kProbeSeconds);
    CHECK(early && work->IndependentRan.load(),
          "unrelated work completes on two workers while the original cook is still blocked");
    if (early) { (void)pool.TakeCompletion(probe); }
    Note(different ? "different-product worker control" : "same-product worker probe",
         early ? 1 : 0,
         "completed before cook release");
  }
  work->Release.release();
  const auto left = Wait(a);
  const auto right = Wait(b);
  CheckCompletions(left, right, second.Address, scenario);
  CHECK(work->Cooks.load() == (different ? size_t{2} : size_t{1}),
        "the same product is cooked once even when its initial consumer cancels");
  CheckGeometry(*pages, second.Address);
  Note(CookLabel(scenario), static_cast<double>(work->Cooks.load()), "producer calls");
}
}

int main() {
  using namespace outshine::Test;
  auto root = (std::filesystem::temp_directory_path() / "outshine-road-consumers-XXXXXX").string();
  CHECK(mkdtemp(root.data()) != nullptr, "the fixture owns its native asset store");
  if (!std::filesystem::is_directory(root)) { return Report(); }
  auto opened = PreparedRoadPages::Open(root);
  CHECK(opened.has_value(), "the real road asset store opens");
  if (!opened) { return Report(); }
  auto pages = std::shared_ptr<PreparedRoadPages>(std::move(*opened));
  CheckSharing(pages, Scenario::DifferentProduct);
  CheckSharing(pages, Scenario::Shared);
  CheckSharing(pages, Scenario::CancelOwner);
  CheckSharing(pages, Scenario::CancelSingleWorker);
  pages.reset();
  std::error_code error;
  std::filesystem::remove_all(root, error);
  CHECK(!error, "workers and consumers end before the isolated assets are removed");
  return Report();
}
