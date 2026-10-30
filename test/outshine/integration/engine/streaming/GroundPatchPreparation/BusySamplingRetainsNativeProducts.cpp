#include "GroundPatchPreparation.h"
#include "AssetCache.h"
#include "PreparedTerrainAssets.h"
#include "PreparedGroundPatchCodec.h"
#include "GroundPatch.h"
#include "GroundSnapshot.h"
#include "GroundQuery.h"
#include "GroundSample.h"
#include "Ground.h"
#include "SourceSet.h"
#include "RawSourceCache.h"
#include "Check.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <sqlite3.h>
#include <stop_token>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <variant>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
using Assets = PreparedTerrainAssets;
constexpr auto kDeadline = std::chrono::seconds(5);
constexpr Data::TileId kAt{.Zoom = 14, .X = 8937, .Y = 5683};
constexpr double kHeight = 100.12345;

class Heights final : public GroundQuery {
public:
  GroundSample At([[maybe_unused]] LongitudeLatitude at) const override {
    ++Samples;
    return Waiting ? GroundSample::Waiting() : GroundSample::At(kHeight);
  }

  GroundSample Resident(LongitudeLatitude at) const override { return At(at); }

  outshine::Ground::GroundBlock
  BlockAt([[maybe_unused]] outshine::Ground::TileSpot spot) const override {
    return {};
  }

  double PostM([[maybe_unused]] double latitudeDeg) const override {
    return Tile(kAt.Zoom, kAt.X, kAt.Y).SpanNm() / 4;
  }

  int BlockZoom() const override { return kAt.Zoom + 1; }

  mutable size_t Samples = 0;
  bool Waiting = false;
};

struct Fixture {
  std::filesystem::path Directory =
      std::filesystem::temp_directory_path() /
      ("outshine-patch-publication-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  Data::RawSourceCache Raw{{.Directory = {}, .Using = Data::RawSourceCache::Use::Off}};
  const Data::SourceSet Sources{Raw};
  Tasks Pool{1};
  std::shared_ptr<Assets> Cache;
  std::unique_ptr<GroundPatchPreparation> Job;
  std::unique_ptr<sqlite3, decltype(&sqlite3_close)> Writer{nullptr, sqlite3_close};
  const Tile Region{kAt.Zoom, kAt.X, kAt.Y};
  std::string Key;

  ~Fixture() {
    Job.reset();
    Barrier();
    Writer.reset();
    Cache.reset();
    std::error_code error;
    std::filesystem::remove_all(Directory, error);
    CHECK(!error, "private native products are removed after workers and cache owners finish");
  }

  bool Open() {
    auto opened = Assets::Open(Directory.string(), Sources);
    if (!opened) { return false; }
    Cache = std::move(*opened);
    Key = Cache->PatchKey(kAt, {}, 5, kAt.Zoom + 1);
    sqlite3 *connection = nullptr;
    const int status = sqlite3_open((Directory / "assets.sqlite").c_str(), &connection);
    Writer.reset(connection);
    return status == SQLITE_OK && !Key.empty();
  }

  void Barrier() {
    const auto barrier = Pool.Post([] {});
    CHECK(barrier != Tasks::kNoTask && Pool.AwaitCompletion(barrier, 5),
          "unrelated work completes on the single worker while native publication is deferred");
    if (barrier != Tasks::kNoTask) { (void)Pool.TakeCompletion(barrier); }
  }

  void Hold() const {
    CHECK(sqlite3_exec(Writer.get(), "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) == SQLITE_OK,
          "an independent SQLite writer holds publication admission");
  }

  void Release() const {
    CHECK(sqlite3_exec(Writer.get(), "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK,
          "the independent writer releases publication admission");
  }

  void Begin(const Heights &heights) {
    Job = std::make_unique<GroundPatchPreparation>(Pool, Cache, Key, Region, 5);
    Barrier();
    Snapped how = Snapped::Taken;
    const auto pending = Job->Advance(heights, &how);
    CHECK(pending && !*pending && how == Snapped::Waiting && heights.Samples == 25,
          "the actual miss samples the required grid once before posting native publication");
  }

  std::expected<std::shared_ptr<const GroundPatch>, std::string>
  Await(const Heights &heights) const {
    const auto until = std::chrono::steady_clock::now() + kDeadline;
    do {
      Snapped how = Snapped::Waiting;
      auto result = Job->Advance(heights, &how);
      if (!result || *result) { return result; }
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    } while (std::chrono::steady_clock::now() < until);
    return std::unexpected("native sampling fixture timed out");
  }
};

bool Busy(const Assets::Failure &failure) {
  const auto *cache = std::get_if<AssetCacheError>(&failure);
  return cache != nullptr && *cache == AssetCacheError::Busy;
}

void CheckBits(const GroundPatch &actual) {
  std::array<GroundPatch::Posting, 25> postings;
  for (auto &posting : postings) { posting.Height = GroundSample::At(kHeight); }
  const auto expected = GroundPatch::Complete(Tile(kAt.Zoom, kAt.X, kAt.Y), 5, postings);
  const auto actualBytes = EncodePreparedGroundPatch(kAt, actual);
  const auto expectedBytes = expected ? EncodePreparedGroundPatch(kAt, *expected) : std::nullopt;
  CHECK(actualBytes && expectedBytes && *actualBytes == *expectedBytes,
        "native publication preserves the complete encoded samples and geographic grid bytewise");
}

void CheckColdAndWarm() {
  Fixture fixture;
  const bool opened = fixture.Open();
  CHECK(opened, "the real sampling cache opens without source acquisition");
  if (!opened) { return; }
  const Heights heights;
  fixture.Hold();
  fixture.Begin(heights);
  fixture.Barrier();
  CHECK(fixture.Cache->Costs().PatchPreparations == 1 &&
            fixture.Cache->Costs().PatchPublicationAttempts == 1 &&
            fixture.Cache->Costs().PatchWrites == 0,
        "the actual worker retains one prepared package at the first real Busy");
  const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(50);
  while (std::chrono::steady_clock::now() < until) {
    Snapped how = Snapped::Taken;
    const auto result = fixture.Job->Advance(heights, &how);
    CHECK(result && !*result && how == Snapped::Waiting && heights.Samples == 25,
          "writer contention remains pending without resampling terrain");
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  fixture.Barrier();
  const auto busyCosts = fixture.Cache->Costs();
  CHECK(busyCosts.PatchPreparations == 1 && busyCosts.PatchDecodes == 0 &&
            busyCosts.PatchPublicationAttempts < 20,
        "bounded deadlines reuse prepared bytes without repeated encoding or native restore");
  fixture.Release();
  const auto ready = fixture.Await(heights);
  CHECK(ready && *ready && heights.Samples == 25 && fixture.Cache->Costs().PatchWrites == 1 &&
            fixture.Cache->Costs().PatchDecodes == 1,
        "the cold product becomes visible only after one commit and validated native restore");
  if (ready && *ready) { CheckBits(**ready); }
  fixture.Job.reset();
  fixture.Barrier();
  auto independent = Assets::Open(fixture.Directory.string(), fixture.Sources);
  CHECK(independent.has_value(),
        "a separate cache owner can read the complete warm native product");
  if (!independent) { return; }
  fixture.Hold();
  Heights unavailable;
  unavailable.Waiting = true;
  {
    GroundPatchPreparation warm(fixture.Pool, *independent, fixture.Key, fixture.Region, 5);
    fixture.Barrier();
    Snapped how = Snapped::Waiting;
    const auto loaded = warm.Advance(unavailable, &how);
    CHECK(loaded && *loaded && how == Snapped::Taken && unavailable.Samples == 0 &&
              (*independent)->Costs().PatchPreparations == 0 &&
              (*independent)->Costs().PatchPublicationAttempts == 0,
          "a warm hit bypasses source queries, preparation and writes even with a held writer");
    if (loaded && *loaded) { CheckBits(**loaded); }
  }
  fixture.Release();
}

void CheckCancellation(bool queued) {
  Fixture fixture;
  const bool opened = fixture.Open();
  CHECK(opened, "the cancelable sampling cache opens");
  if (!opened) { return; }
  const Heights heights;
  fixture.Hold();
  fixture.Begin(heights);
  if (!queued) {
    fixture.Barrier();
    CHECK(fixture.Cache->Costs().PatchPublicationAttempts == 1,
          "idle cancellation starts from a verified actual writer-Busy step");
  }
  fixture.Job.reset();
  fixture.Barrier();
  CHECK(fixture.Cache.use_count() == 1 && fixture.Cache->Costs().PatchWrites == 0,
        "cancellation releases worker cache owners without requiring another Advance");
  fixture.Release();
  const auto absent = fixture.Cache->LoadPatch(fixture.Key, kAt, 5);
  CHECK(absent && !*absent,
        "a canceled prepared package is never published after the lock releases");
}

void CheckPermanentFailure() {
  Fixture fixture;
  const bool opened = fixture.Open();
  CHECK(opened, "the permanent-failure cache opens");
  if (!opened) { return; }
  CHECK(sqlite3_exec(fixture.Writer.get(),
                     "CREATE TRIGGER reject_sampling BEFORE INSERT ON assets BEGIN "
                     "SELECT RAISE(ABORT,'busy text in a permanent constraint'); END;",
                     nullptr,
                     nullptr,
                     nullptr) == SQLITE_OK,
        "a native constraint supplies a permanent storage error independently of Busy");
  const Heights heights;
  fixture.Begin(heights);
  const auto failed = fixture.Await(heights);
  CHECK(!failed && failed.error() == "terrain asset cache storage failed" &&
            fixture.Cache->Costs().PatchPublicationAttempts == 1 && heights.Samples == 25,
        "permanent native failures become terminal without resampling or retrying error text");
  for (int poll = 0; poll < 3; ++poll) {
    Snapped how = Snapped::Waiting;
    const auto again = fixture.Job->Advance(heights, &how);
    CHECK(!again && fixture.Cache->Costs().PatchPublicationAttempts == 1,
          "terminal observations do not enqueue more native writes");
  }
}

void CheckOwnedBytes() {
  Fixture fixture;
  const bool opened = fixture.Open();
  CHECK(opened, "the owning-byte case uses its own native store");
  if (!opened) { return; }
  const std::array<double, 9> values{123.45678901234567,
                                     -0.0,
                                     0.0,
                                     -11500.0,
                                     9500.0,
                                     80.0,
                                     0.000000001,
                                     -123.76543210987654,
                                     1234.12345};
  std::array<GroundPatch::Posting, 9> postings;
  for (size_t i = 0; i < values.size(); ++i) { postings[i].Height = GroundSample::At(values[i]); }
  auto original = GroundPatch::Complete(fixture.Region, 3, postings);
  CHECK(original != nullptr, "the original native patch owns exact finite double samples");
  if (!original) { return; }
  const auto expected = EncodePreparedGroundPatch(kAt, *original);
  const auto key = fixture.Cache->PatchKey(kAt, {}, 3, kAt.Zoom + 1);
  const std::weak_ptr<const GroundPatch> owner = original;
  auto write = fixture.Cache->PreparePatch(key, kAt, *original);
  CHECK(write.has_value(), "patch preparation owns its normalized package independently of input");
  if (!write) { return; }
  original.reset();
  const auto capacity = write->PublicationBytes();
  CHECK(owner.expired() && capacity > 0, "prepared publication does not borrow the sampled input");
  fixture.Hold();
  for (size_t step = 0; step < 4; ++step) {
    const auto pending = fixture.Cache->StorePatch(*write);
    CHECK(!pending && Busy(pending.error()) && write->PublicationBytes() == capacity &&
              fixture.Cache->Costs().PatchPreparations == 1 &&
              fixture.Cache->Costs().PatchPublicationAttempts == step + 1,
          "repeated Busy reuses one immutable owned package after original inputs are released");
  }
  fixture.Release();
  const auto ready = fixture.Cache->StorePatch(*write);
  CHECK(ready && *ready && write->PublicationBytes() == 0,
        "validated restoration releases the prepared package owner");
  if (!ready || !*ready) { return; }
  const auto restored = EncodePreparedGroundPatch(kAt, **ready);
  CHECK(expected && restored && *expected == *restored,
        "all sample bits including signed zero survive Busy, commit and validated decode");
  const auto before = fixture.Cache->Costs();
  fixture.Hold();
  const auto repeated = fixture.Cache->StorePatch(*write);
  CHECK(repeated && *repeated == *ready &&
            fixture.Cache->Costs().PatchDecodes == before.PatchDecodes &&
            fixture.Cache->Costs().PatchPublicationAttempts == before.PatchPublicationAttempts,
        "a completed write returns its same product without IO or decode even under a writer lock");
  std::stop_source stop;
  (void)stop.request_stop();
  const auto canceled = fixture.Cache->StorePatch(*write, stop.get_token());
  CHECK(!canceled && !Busy(canceled.error()),
        "stopped observation remains terminal without cache IO");
  fixture.Release();
  Note("prepared sampling package capacity", static_cast<double>(capacity), "bytes");
}

void CheckForeignKind() {
  Fixture fixture;
  const bool opened = fixture.Open();
  CHECK(opened, "the ownership counterexample opens a native store");
  if (!opened) { return; }
  auto other = AssetCache::Open((fixture.Directory / "assets.sqlite").string());
  CHECK(other.has_value(), "another product owner uses the same asset database");
  if (!other) { return; }
  auto write = (*other)->PreparePublication({{.Key = fixture.Key,
                                              .Kind = "another-native-product",
                                              .Bounds = {.Min = {{0, 0, 0}}, .Max = {{1, 1, 1}}},
                                              .Package = {},
                                              .ByteCount = 3,
                                              .Parent = {}}},
                                            {1, 2, 3});
  CHECK(write && (*other)->Publish(*write), "the other owner publishes a valid native product");
  const auto refused = fixture.Cache->LoadPatch(fixture.Key, kAt, 5);
  const auto retained = (*other)->Load(fixture.Key, 3);
  CHECK(!refused && !Busy(refused.error()) && retained && *retained &&
            (**retained).Record().Kind == "another-native-product" &&
            (**retained).Bytes().size() == 3 && (**retained).Bytes().front() == 1 &&
            (**retained).Bytes().back() == 3,
        "a conflicting native format is terminal and never deletes another owner's valid product");
}
}

int main() {
  CheckColdAndWarm();
  CheckCancellation(false);
  CheckCancellation(true);
  CheckPermanentFailure();
  CheckOwnedBytes();
  CheckForeignKind();
  return outshine::Test::Report();
}
