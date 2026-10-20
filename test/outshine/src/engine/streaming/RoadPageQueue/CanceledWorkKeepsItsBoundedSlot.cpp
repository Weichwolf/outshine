#include "RoadPageQueue.h"
#include "ProfiledRoadMesher.h"
#include "TangentFrame.h"
#include "Check.h"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <memory>
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

RoadPageAddress Address(uint32_t id) {
  return {.PlanKey = std::string(64, 'a'),
          .MaterialKey = std::string(64, 'b'),
          .MaterialIndex = 0,
          .Anchor = {.LongitudeDeg = 8, .LatitudeDeg = 49},
          .Level = 0,
          .Parts = {{.Kind = RoadPagePartKind::Interval, .Index = id}}};
}

std::expected<RoadPageGeometry, std::string> Produce(uint32_t edge = 7) {
  RoadPageRecipe recipe;
  recipe.Intervals.push_back({.Edge = edge,
                              .Lane = 0,
                              .Stations = {{.EastM = 0, .NorthM = 0, .GradeM = 0, .Node = 1},
                                           {.EastM = 100, .NorthM = 0, .GradeM = 0, .Node = 2}},
                              .Surface = {.HalfWidthM = 4,
                                          .Profile = RoadProfile::Kerbed,
                                          .WearsLinear = {{0.5f, 0.5f, 0.5f}},
                                          .Crossfall = 0,
                                          .Form = RibbonForm::Surface,
                                          .EndPorts = {}}});
  const ProfiledRoadMesher mesher;
  auto geometry = MeshRoadPage(mesher, recipe);
  if (!geometry) { return std::unexpected(std::string(geometry.error())); }
  return std::move(*geometry);
}

std::vector<RoadPageQueue::Completed> Wait(RoadPageQueue &queue) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (std::chrono::steady_clock::now() < deadline) {
    auto done = queue.Collect();
    if (!done.empty()) { return done; }
    std::this_thread::yield();
  }
  return {};
}

const PreparedRoadPages::Loaded *Payload(const std::vector<RoadPageQueue::Completed> &done) {
  if (done.size() != 1 || !done[0].Result || !*done[0].Result) { return nullptr; }
  return std::get_if<PreparedRoadPages::Loaded>(&**done[0].Result);
}

void CheckQueue(Tasks &pool, const std::shared_ptr<PreparedRoadPages> &pages) {
  using namespace outshine::Test;
  RoadPageQueue queue(pool, pages, {.PendingMost = 1, .PageBytesMost = 1024u * 1024u});

  struct Control {
    std::binary_semaphore Entered{0};
    std::binary_semaphore Finish{0};
    std::atomic<size_t> Factories{0};
  };

  const auto control = std::make_shared<Control>();
  const auto first = Address(7);
  const auto other = Address(8);
  const RoadPageQueue::Factory blocked =
      [control](size_t,
                const std::stop_token &stop) -> std::expected<RoadPageGeometry, std::string> {
    ++control->Factories;
    control->Entered.release();
    control->Finish.acquire();
    if (stop.stop_requested()) { return std::unexpected("canceled fixture producer"); }
    return Produce();
  };
  CHECK(queue.Request(first, {}, {}, blocked), "the first native request occupies one worker slot");
  const bool running = control->Entered.try_acquire_for(std::chrono::seconds(5));
  CHECK(running, "the bounded page producer is running on the actual task pool");
  if (!running) {
    control->Finish.release();
    return;
  }
  CHECK(queue.Request(first, {}, {}, blocked) && control->Factories.load() == 1 &&
            queue.Pending() == 1,
        "identical pending requests are coalesced before IO or generation");
  CHECK(queue.RequestCached(RoadPageKey(first)) && queue.Pending() == 1 &&
            control->Factories.load() == 1,
        "a metadata demand shares an already running producer for the same product");
  CHECK(!queue.RequestMetadata(first) && queue.Pending() == 1,
        "an index probe cannot silently coalesce with a different payload result type");
  CHECK(!queue.RequestCached(RoadPageKey(other)) && !queue.Request(other, {}, {}, {}) &&
            queue.Collect().empty(),
        "the frame cannot enqueue beyond its bound or wait for a blocked producer");
  queue.Cancel(RoadPageKey(first));
  CHECK(!queue.Request(other, {}, {}, {}) && !queue.Request(first, {}, {}, blocked) &&
            queue.Pending() == 1,
        "canceled running work retains its slot until actual completion");
  control->Finish.release();
  auto canceled = Wait(queue);
  CHECK(canceled.size() == 1 && !canceled[0].Result && queue.Pending() == 0,
        "the canceled product is not published to the consumer");
  const auto missing = pages->Load(first, 1024u * 1024u);
  CHECK(missing && !*missing, "cancellation before encoding publishes no partial cached page");
  auto source = std::make_shared<uint32_t>(8);
  const std::weak_ptr<uint32_t> retainedSource = source;
  CHECK(queue.Request(other,
                      {},
                      {},
                      [source = std::move(source)](size_t, const std::stop_token &) {
                        return Produce(*source);
                      }),
        "completed cancellation releases capacity for the next needed page");
  const auto generated = pool.Post([] {});
  CHECK(generated != Tasks::kNoTask, "a worker barrier observes completed native generation");
  if (generated == Tasks::kNoTask) { return; }
  pool.Wait(generated);
  CHECK(retainedSource.expired(), "worker completion releases generator inputs before collection");
  auto done = Wait(queue);
  CHECK(Payload(done) && !Payload(done)->Page->Geometry.Mesh.Index.empty(),
        "completed work exposes an independently owned native page");
  const auto retained =
      Payload(done) ? Payload(done)->Page : std::shared_ptr<const RoadPageAsset>{};
  CHECK(queue.Request(other, {}, {}, {}), "a cached page can be requested without any generator");
  auto hit = Wait(queue);
  CHECK(Payload(hit) && retained && Payload(hit)->Record.Key == RoadPageKey(other) &&
            control->Factories.load() == 1,
        "a warm asynchronous request restores the existing page without source preparation");
  CHECK(queue.RequestCached(RoadPageKey(other)),
        "a metadata-only hit can enter a new demand generation");
  const auto barrier = pool.Post([] {});
  CHECK(barrier != Tasks::kNoTask, "a worker barrier can observe completed cache restoration");
  if (barrier == Tasks::kNoTask) { return; }
  pool.Wait(barrier);
  queue.Cancel(RoadPageKey(other));
  const auto retired = queue.Collect();
  CHECK(retired.size() == 1 && !retired[0].Result,
        "a canceled generation cannot admit an already completed page");
}

void CheckMetadata(Tasks &pool, const std::filesystem::path &directory) {
  using namespace outshine::Test;
  auto metadata = AssetCache::Open((directory / "assets.sqlite").string());
  CHECK(metadata.has_value(), "the independent metadata connection opens");
  if (!metadata) { return; }
  const auto frame = TangentFrame::At({.LongitudeDeg = 8, .LatitudeDeg = 49});
  const Vec3 extent{{200, 200, 200}};
  const Box envelope{.Min = frame.OriginEcef() - extent, .Max = frame.OriginEcef() + extent};
  const auto rows = (*metadata)->Select({.Bounds = envelope,
                                         .Kind = "road-render-page",
                                         .Level = std::nullopt,
                                         .Radius = std::nullopt,
                                         .Planes = {}});
  CHECK(rows && rows->size() == 1, "the existing spatial index returns the prepared page metadata");
  if (!rows || rows->size() != 1) { return; }
  auto opened = PreparedRoadPages::Open(directory.string());
  CHECK(opened.has_value(), "the consumer starts with a fresh page service");
  if (!opened) { return; }
  auto pages = std::shared_ptr<PreparedRoadPages>(std::move(*opened));
  RoadPageQueue queue(pool, pages, {.PendingMost = 1, .PageBytesMost = 1024u * 1024u});
  CHECK(!queue.RequestCached("") && !queue.RequestCached(std::string(64, 'g')) &&
            queue.Pending() == 0,
        "invalid metadata keys are rejected without occupying worker slots");
  const auto &selected = rows->front();
  CHECK(queue.RequestCached(selected.Key) && queue.RequestCached(selected.Key) &&
            queue.Pending() == 1,
        "metadata alone enqueues and coalesces one bounded cached-page load");
  auto hit = Wait(queue);
  CHECK(Payload(hit) && Payload(hit)->Record == selected &&
            !Payload(hit)->Page->Geometry.Mesh.Index.empty(),
        "a fresh worker restores the selected native page without any plan address or factory");
  CHECK(queue.RequestMetadata(Address(8)) && queue.RequestMetadata(Address(8)) &&
            !queue.RequestCached(selected.Key) && queue.Pending() == 1,
        "index probes coalesce while a fine load requires its own explicit result contract");
  const auto probe = Wait(queue);
  const auto *record = probe.size() == 1 && probe[0].Result && *probe[0].Result
                           ? std::get_if<AssetRecord>(&**probe[0].Result)
                           : nullptr;
  CHECK(record && *record == selected && !Payload(probe),
        "the metadata result carries the indexed product without a native mesh lease");
  CHECK(queue.RequestCached(std::string(64, 'f')), "an absent valid product can be queried");
  const auto miss = Wait(queue);
  CHECK(miss.size() == 1 && miss[0].Result && !*miss[0].Result,
        "an absent product is a typed normal miss without source or generator work");
  RoadPageQueue limited(pool, pages, {.PendingMost = 1, .PageBytesMost = 1});
  CHECK(limited.RequestCached(selected.Key), "the same metadata can be queried under a byte limit");
  const auto excess = Wait(limited);
  CHECK(excess.size() == 1 && !excess[0].Result,
        "metadata-only loading preserves the complete-package byte budget");
}

}

int main() {
  using namespace outshine::Test;
  const auto root = std::filesystem::temp_directory_path() /
                    ("outshine-road-page-queue-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(root);
  auto opened = PreparedRoadPages::Open(root.string());
  CHECK(opened.has_value(), "the actual native page cache opens");
  if (!opened) { return Report(); }
  auto pages = std::shared_ptr<PreparedRoadPages>(std::move(*opened));
  {
    Tasks pool(1);
    CheckQueue(pool, pages);
    auto distant = Address(9);
    distant.Anchor.LongitudeDeg = 9;
    CHECK(pages->Resolve(distant, {}, {}, 1024u * 1024u, [](size_t) { return Produce(9); })
              .has_value(),
          "a second complete native package lies outside the metadata query");
    CheckMetadata(pool, root);
  }
  pages.reset();
  std::error_code error;
  std::filesystem::remove_all(root, error);
  CHECK(!error, "the owned async cache is cleaned after its workers finish");
  return Report();
}
