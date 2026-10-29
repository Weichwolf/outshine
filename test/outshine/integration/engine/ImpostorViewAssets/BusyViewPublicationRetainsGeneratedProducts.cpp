#include "ImpostorViewJob.h"
#include "AssetPipeline.h"
#include "Check.h"
#include "ImpostorAtlas.h"
#include "ImpostorSurface.h"
#include "Material.h"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <memory>
#include <initializer_list>
#include <optional>
#include <sqlite3.h>
#include <stop_token>
#include <string>
#include <system_error>
#include <thread>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Test;
using Views = Content::ImpostorViewAssets;
constexpr size_t kBytes = size_t{1024} * 1024;
constexpr auto kDeadline = std::chrono::seconds(5);

struct Fixture {
  std::filesystem::path Directory =
      std::filesystem::temp_directory_path() /
      ("outshine-view-busy-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::shared_ptr<AssetCache> Cache;
  std::unique_ptr<sqlite3, decltype(&sqlite3_close)> Writer{nullptr, sqlite3_close};

  ~Fixture() {
    Writer.reset();
    Cache.reset();
    std::error_code error;
    std::filesystem::remove_all(Directory, error);
    CHECK(!error, "private view products are removed after their native owners close");
  }

  std::optional<Views> Open() {
    std::filesystem::create_directories(Directory);
    const auto path = (Directory / "assets.sqlite").string();
    auto cache = AssetCache::Open(path);
    CHECK(cache, "the actual native view store opens");
    if (!cache) { return std::nullopt; }
    Cache = std::move(*cache);
    auto views =
        Views::Publish(*Cache,
                       "busy-view-source",
                       {.Centre = {}, .HalfExtentM = 1, .Pixels = 8, .Directions = {{{0, 0, 1}}}},
                       {.Min = {{-1, -1, -1}}, .Max = {{1, 1, 1}}});
    CHECK(views, "the real native metadata exists before contention");
    if (!views) { return std::nullopt; }
    sqlite3 *raw = nullptr;
    const int status = sqlite3_open(path.c_str(), &raw);
    Writer.reset(raw);
    CHECK(status == SQLITE_OK, "a second SQLite writer opens the same store");
    if (status != SQLITE_OK) { return std::nullopt; }
    return std::move(*views);
  }

  void Hold() const {
    CHECK(sqlite3_exec(Writer.get(), "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) == SQLITE_OK,
          "the second writer holds only native publication admission");
  }

  void Release() const {
    CHECK(sqlite3_exec(Writer.get(), "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK,
          "the independent writer releases publication admission");
  }
};

std::expected<ImpostorAtlas, std::string> Field() {
  ImpostorAtlas::View view{.TowardEye = {{0, 0, 1}}, .Texels = {}, .Materials = {}};
  view.Texels.resize(64, {.Normal = {{0, 0, 1}}, .Depth = .5f, .Surface = 1});
  std::string error;
  auto field = ImpostorAtlas::Create(8, {}, 1, {Material{}}, {std::move(view)}, error);
  if (!field) { return std::unexpected(error); }
  return std::move(*field);
}

template <bool MaterialField> auto Generated(auto selected) {
  if constexpr (MaterialField) {
    return Field();
  } else {
    auto field = Field();
    if (!field) { return std::expected<Geometry, std::string>(std::unexpected(field.error())); }
    auto geometry = Render::BuildFractionalImpostorSurface(*field, selected.View, selected.Mip);
    if (!geometry) {
      return std::expected<Geometry, std::string>(
          std::unexpected("native surface conversion failed"));
    }
    return std::expected<Geometry, std::string>(std::move(*geometry));
  }
}

enum class Completion { Resume, Cancel, MetadataChanged };

bool Idle(const AssetPipeline &pipeline) {
  const auto until = std::chrono::steady_clock::now() + kDeadline;
  while (pipeline.Counts().Running != 0 && std::chrono::steady_clock::now() < until) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  CHECK(pipeline.Counts().Running == 0,
        "the prepared job releases its worker before the next phase");
  return pipeline.Counts().Running == 0;
}

void Replay(Fixture &fixture, const ImpostorViewJob &job, const std::atomic<size_t> &calls) {
  fixture.Hold();
  const auto warm = job.Prepare()({});
  CHECK(warm && *warm && calls.load(std::memory_order_relaxed) == 1,
        "a validated product hit remains source-free and read-only under another writer");
  fixture.Release();
  auto cache = AssetCache::Open((fixture.Directory / "assets.sqlite").string());
  CHECK(cache, "a fresh native store owner opens the completed publication");
  if (!cache) { return; }
  auto fresh = job;
  fresh.Cache = std::move(*cache);
  const auto restored = fresh.Prepare()({});
  CHECK(restored && *restored && warm && *warm && **restored == **warm &&
            calls.load(std::memory_order_relaxed) == 1,
        "a fresh process-local owner validates the identical native product without its producer");
}

template <bool MaterialField> void Resume(Completion completion) {
  Fixture fixture;
  const auto views = fixture.Open();
  if (!views) { return; }
  const auto key = MaterialField ? views->FieldKey({0}) : views->Key({.View = 0, .Mip = 1});
  Tasks workers(1);
  AssetPipeline pipeline(workers,
                         {.JobsMost = 1,
                          .ConsumersMost = 1,
                          .TicketsMost = 1,
                          .InputBytesMost = 128,
                          .ReceiptBytesMost = 2048});
  auto consumer = pipeline.OpenConsumer({.Identity = fixture.Cache});
  CHECK(consumer, "the real publication consumer opens");
  if (!consumer) { return; }
  const auto calls = std::make_shared<std::atomic<size_t>>(0);
  ImpostorViewJob job{.Cache = fixture.Cache,
                      .Provenance = "busy-view-source",
                      .ProductKey = key,
                      .BytesMost = kBytes,
                      .Product = {}};
  const auto generate = [calls](auto selected, size_t, const std::stop_token &) {
    calls->fetch_add(1, std::memory_order_relaxed);
    return Generated<MaterialField>(selected);
  };
  if constexpr (MaterialField) {
    job.Product = ImpostorViewJob::MaterialView{.Selected = {0}, .Generate = generate};
  } else {
    job.Product =
        ImpostorViewJob::SurfaceView{.Selected = {.View = 0, .Mip = 1}, .Generate = generate};
  }
  fixture.Hold();
  auto ticket =
      pipeline.RequestResumable(*consumer,
                                {.Key = AssetProductKey::Parse(key).value_or(AssetProductKey{}),
                                 .Generation = consumer->Generation(),
                                 .InputBytes = 128,
                                 .ConsumerBytes = 0,
                                 .ReceiptBytesMost = 2048},
                                job.Prepare());
  CHECK(ticket, "the generated view is admitted while only its writer is unavailable");
  if (!ticket) { return; }
  const auto until = std::chrono::steady_clock::now() + kDeadline;
  AssetPublication held;
  while (calls->load(std::memory_order_relaxed) == 0 && !held.Finished &&
         std::chrono::steady_clock::now() < until) {
    held = ticket->Poll();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  if (!Idle(pipeline)) { return; }
  const auto blockedUntil = std::chrono::steady_clock::now() + std::chrono::milliseconds(40);
  while (!held.Finished && std::chrono::steady_clock::now() < blockedUntil) {
    held = ticket->Poll();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  CHECK(
      !held.Finished && calls->load(std::memory_order_relaxed) == 1 &&
          pipeline.Counts().Jobs == 1 && pipeline.Counts().InputBytes == 128,
      "a complete generated view remains owned in the same admitted job during writer contention");
  if (completion == Completion::Cancel) { ticket->Cancel(); }
  if (!Idle(pipeline)) { return; }
  fixture.Release();
  if (completion == Completion::Cancel) {
    const auto cancelled = ticket->Poll();
    ticket = std::unexpected(AssetPublicationError::InvalidRequest);
    const auto absent = fixture.Cache->Find(key);
    CHECK(cancelled.Status == AssetPublicationStatus::Cancelled && absent && !*absent &&
              pipeline.Counts().InputBytes == 0 && calls->load(std::memory_order_relaxed) == 1,
          "cancellation releases the admitted packet and budget without a late publication");
    return;
  }
  if (completion == Completion::MetadataChanged) {
    const auto changed =
        Views::Publish(*fixture.Cache,
                       "busy-view-source",
                       {.Centre = {}, .HalfExtentM = 2, .Pixels = 8, .Directions = {{{0, 0, 1}}}},
                       {.Min = {{-1, -1, -1}}, .Max = {{1, 1, 1}}});
    CHECK(changed, "another owner publishes new metadata after the old product was prepared");
    if (!changed) { return; }
  }
  AssetPublication ready;
  do {
    ready = ticket->Poll();
    if (ready.Finished) { break; }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  } while (std::chrono::steady_clock::now() < until);
  if (completion == Completion::MetadataChanged) {
    const auto absent = fixture.Cache->Find(key);
    CHECK(ready.Finished && ready.Status == AssetPublicationStatus::Failed && absent && !*absent &&
              calls->load(std::memory_order_relaxed) == 1,
          "changed metadata rejects a stale prepared packet without publication or regeneration");
    return;
  }
  CHECK(ready.Finished && ready.Status == AssetPublicationStatus::Ready && ready.Receipt &&
            ready.Receipt->Key == key && calls->load(std::memory_order_relaxed) == 1,
        "writer release publishes the retained complete product without recapture or regeneration");
  Replay(fixture, job, *calls);
}
}

int main() {
  for (const auto completion :
       {Completion::Resume, Completion::Cancel, Completion::MetadataChanged}) {
    Resume<false>(completion);
    Resume<true>(completion);
  }
  return Report();
}
