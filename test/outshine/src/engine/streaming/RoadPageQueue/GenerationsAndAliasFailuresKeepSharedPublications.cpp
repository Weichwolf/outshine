#include "RoadPublicationFixture.h"
#include "ControlledRoadPageProducer.h"
#include "Check.h"
#include <sqlite3.h>
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
using namespace outshine::Test;
constexpr size_t kLimit = size_t{1024} * 1024;
constexpr auto kDeadline = std::chrono::seconds(5);
constexpr auto kPoll = std::chrono::milliseconds(1);
constexpr uint32_t kPart = 7;
constexpr double kLatitude = 49;

RoadPageRequest Request(char plan, std::string alias) {
  return {.Address = {.PlanKey = std::string(64, plan),
                      .MaterialKey = std::string(64, 'b'),
                      .MaterialIndex = 0,
                      .Anchor = {.LongitudeDeg = 8, .LatitudeDeg = kLatitude},
                      .Level = 0,
                      .Parts = {{.Kind = RoadPagePartKind::Interval, .Index = kPart}}},
          .Parent = {},
          .RequestKey = std::move(alias),
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

std::vector<RoadPageQueue::Completed> Wait(RoadPageQueue &queue) {
  const auto until = std::chrono::steady_clock::now() + kDeadline;
  do {
    auto done = queue.Collect();
    if (!done.empty()) { return done; }
    std::this_thread::sleep_for(kPoll);
  } while (std::chrono::steady_clock::now() < until);
  return {};
}

const AssetRecord *Receipt(const std::vector<RoadPageQueue::Completed> &done) {
  if (done.size() != 1) { return nullptr; }
  const auto &result = done.front().Result;
  if (!result || !*result) { return nullptr; }
  const auto *record = std::get_if<RoadPageQueue::Receipt>(&**result);
  return record != nullptr && *record ? record->get() : nullptr;
}

void CheckRebind(const std::shared_ptr<PreparedRoadPages> &pages) {
  const auto producer =
      RoadProducer([](const RoadPageRequest &, const std::stop_token &) { return Triangle(); });
  const auto first = Request('a', {});
  const auto second = Request('c', {});
  CHECK(pages->Prepare(first, producer.get()) && pages->Prepare(second, producer.get()),
        "both immutable native products exist before the asynchronous binding race");
  Tasks pool(1);
  const auto pipeline = RoadPublications(pool);
  RoadPageQueue queue(pool, pages, {.PendingMost = 2, .PageBytesMost = kLimit}, pipeline);
  auto old = first;
  old.RequestKey.assign(64, 'd');
  CHECK(AdmitRoadPage(queue, old), "the old generation requests its own durable alias");
  const auto published = pool.Post([] {});
  CHECK(published != Tasks::kNoTask, "a real pool barrier follows the publication lookup");
  if (published == Tasks::kNoTask) { return; }
  pool.Wait(published);

  struct Block {
    std::binary_semaphore Started{0}, Release{0};
  };

  const auto block = std::make_shared<Block>();
  CHECK(pool.PostDetached([block] {
    block->Started.release();
    block->Release.acquire();
  }),
        "a controlled task delays the old alias without holding an asset lock");
  const bool started = block->Started.try_acquire_for(kDeadline);
  CHECK(started, "the old alias cannot run before the generation changes");
  if (!started) {
    block->Release.release();
    return;
  }
  CHECK(queue.Collect().empty() && queue.Pending() == 1,
        "polling queues asynchronous metadata work without performing its SQLite IO");
  CHECK(queue.SetGeneration({.Value = 2}), "a new meaning cancels the old consumer generation");
  auto current = second;
  current.RequestKey = old.RequestKey;
  CHECK(AdmitRoadPage(queue, current), "the current generation admits its replacement binding");
  block->Release.release();
  std::vector<RoadPageQueue::Completed> done;
  const auto until = std::chrono::steady_clock::now() + kDeadline;
  size_t canceled = 0;
  while (std::chrono::steady_clock::now() < until && Receipt(done) == nullptr) {
    auto batch = queue.Collect();
    for (auto &entry : batch) {
      if (!entry.Result) {
        ++canceled;
      } else {
        done.push_back(std::move(entry));
      }
    }
    std::this_thread::sleep_for(kPoll);
  }
  const auto *bound = Receipt(done);
  CHECK(canceled == 1 && bound && bound->Key == RoadPageKey(second.Address) &&
            bound->RequestKey == current.RequestKey,
        "only the current generation supplies the replacement alias result");
  const auto loaded = pages->LoadRequest(current.RequestKey, kLimit);
  CHECK(loaded && *loaded && (**loaded).Record.Key == RoadPageKey(second.Address),
        "the canceled old alias cannot overwrite the current durable binding");
  const auto original = pages->Load(first.Address, kLimit);
  CHECK(original && *original,
        "generation cancellation preserves the original valid shared product");
}

void CheckAliasFailure(const std::filesystem::path &root,
                       const std::shared_ptr<PreparedRoadPages> &pages) {
  sqlite3 *database = nullptr;
  const int opened = sqlite3_open((root / "assets.sqlite").c_str(), &database);
  CHECK(opened == SQLITE_OK, "the fixture opens an independent native metadata connection");
  if (opened != SQLITE_OK) {
    if (database != nullptr) { sqlite3_close(database); }
    return;
  }
  const int trigger = sqlite3_exec(database,
                                   "CREATE TRIGGER refuse_alias BEFORE INSERT ON asset_requests "
                                   "BEGIN SELECT RAISE(FAIL,'controlled alias failure'); END;",
                                   nullptr,
                                   nullptr,
                                   nullptr);
  sqlite3_close(database);
  CHECK(trigger == SQLITE_OK, "the actual store can reject only the final alias binding");
  if (trigger != SQLITE_OK) { return; }
  Tasks pool(2);
  const auto pipeline = RoadPublications(pool);
  RoadPageQueue aliased(pool, pages, {.PendingMost = 1, .PageBytesMost = kLimit}, pipeline);
  RoadPageQueue direct(pool, pages, {.PendingMost = 1, .PageBytesMost = kLimit}, pipeline);

  struct Work {
    std::binary_semaphore Started{0}, Release{0};
    std::atomic<size_t> Cooks{0};
  };

  const auto work = std::make_shared<Work>();
  const auto producer = RoadProducer([work](const RoadPageRequest &, const std::stop_token &) {
    ++work->Cooks;
    work->Started.release();
    work->Release.acquire();
    return Triangle();
  });
  const auto request = Request('e', std::string(64, 'f'));
  CHECK(AdmitRoadPage(aliased, request, producer),
        "the alias consumer owns the first actual production");
  const bool started = work->Started.try_acquire_for(kDeadline);
  CHECK(started, "the real producer remains blocked until its second consumer is admitted");
  if (!started) {
    work->Release.release();
    return;
  }
  auto plain = request;
  plain.RequestKey.clear();
  CHECK(AdmitRoadPage(direct, plain, producer),
        "a separate consumer shares publication without this alias");
  work->Release.release();
  const auto failed = Wait(aliased);
  const auto ready = Wait(direct);
  CHECK(failed.size() == 1 && !failed.front().Result && Receipt(ready) && work->Cooks.load() == 1,
        "failed asynchronous alias binding leaves the other consumer's single shared publication "
        "valid");
  const auto loaded = pages->Load(plain.Address, kLimit);
  CHECK(loaded && *loaded && (**loaded).Page->Geometry.Mesh.Index == Triangle().Mesh.Index,
        "the normal payload loader retains the published native geometry after an alias error");
}
}

int main() {
  auto root =
      (std::filesystem::temp_directory_path() / "outshine-alias-generation-XXXXXX").string();
  CHECK(mkdtemp(root.data()) != nullptr, "the fixture owns isolated native assets");
  auto opened = PreparedRoadPages::Open(root);
  CHECK(opened.has_value(), "the actual native page service opens");
  if (!opened) { return Report(); }
  auto pages = std::shared_ptr<PreparedRoadPages>(std::move(*opened));
  CheckRebind(pages);
  CheckAliasFailure(root, pages);
  pages.reset();
  std::error_code error;
  std::filesystem::remove_all(root, error);
  CHECK(!error, "all workers and leases end before native test data is removed");
  return Report();
}
