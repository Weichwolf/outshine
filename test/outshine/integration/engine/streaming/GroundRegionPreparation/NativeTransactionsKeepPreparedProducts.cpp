#include "test/harness/engine/GroundPublicationFixture.h"
#include "GroundRegionAsset.h"
#include "PreparedRoadPages.h"
#include "Check.h"
#include <expected>
#include <memory>
#include <optional>
#include <sqlite3.h>
#include <string>
#include <stop_token>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;
using namespace GroundPublication;

void ColdThenWarmBind() {
  Fixture fixture;
  const bool opened = fixture.Open();
  CHECK(opened, "real native region/cache inputs open");
  if (!opened) { return; }
  const auto expected = EncodeGroundRegionAsset(*fixture.Region, Regions::PackageBytesMost);
  auto work = fixture.Cache->PrepareStore(fixture.Key,
                                          GroundRegionBoundsEcef(*fixture.Region),
                                          *fixture.Region,
                                          *fixture.Water,
                                          fixture.Request);
  CHECK(work && expected && work->PublicationBytes() > 0,
        "cold region products prepare one bounded owning package");
  if (!work || !expected) { return; }
  const std::weak_ptr<const GroundRegionAsset> original = fixture.Region;
  fixture.Region.reset();
  fixture.Water.reset();
  CHECK(original.expired(), "owned bytes do not retain the generated source region");
  fixture.Hold();
  const auto bytes = work->PublicationBytes();
  for (unsigned attempt = 0; attempt < 3; ++attempt) {
    const auto pending = fixture.Cache->Store(*work);
    CHECK(!pending && Busy(pending.error()) && work->PublicationBytes() == bytes &&
              fixture.Cache->Costs().Preparations == 1,
          "cold Busy retries retain identical capacity without another encode/compress");
  }
  fixture.Release();
  const auto ready = fixture.Cache->Store(*work);
  CHECK(ready && work->PublicationBytes() == 0,
        "successful native commit and validated restore release the prepared package");
  if (!ready) { return; }
  const auto actual = EncodeGroundRegionAsset(ready->Region, Regions::PackageBytesMost);
  CHECK(actual && *actual == *expected,
        "resumed region bytes exactly preserve the native source product");
  const std::string alias = Sha256Hex("ground binding/new request alias");
  const GroundRegionAsset unused;
  const Generators::WaterAsset water;
  fixture.Hold();
  auto warm = fixture.Cache->PrepareStore(fixture.Key, {}, unused, water, alias);
  CHECK(warm && warm->PublicationBytes() == 0 && fixture.Cache->Costs().Preparations == 1,
        "a warm content hit ignores invalid unused generation inputs and retains only decoded "
        "content");
  if (!warm) {
    fixture.Release();
    return;
  }
  const auto costs = fixture.Cache->Costs();
  for (unsigned attempt = 0; attempt < 3; ++attempt) {
    const auto pending = fixture.Cache->Store(*warm);
    CHECK(!pending && Busy(pending.error()) && fixture.Cache->Costs().Decodes == costs.Decodes &&
              fixture.Cache->Costs().ReadBytes == costs.ReadBytes,
          "busy binding retains the actual decoded region without reading/decompressing it again");
  }
  fixture.Release();
  const auto bound = fixture.Cache->Store(*warm);
  CHECK(bound && fixture.Cache->Costs().Decodes == costs.Decodes,
        "admission release binds and returns the retained native region without another decode");
  auto fresh = Regions::Open(fixture.Directory.string(), fixture.Sources, "fixture-rules");
  CHECK(fresh, "a fresh native owner can verify the persisted request binding");
  if (!fresh) { return; }
  const auto hit = (*fresh)->LoadRequest(alias);
  CHECK(hit && *hit && (**hit).Key == fixture.Key,
        "the exact warm request alias survives independently of the original owner");
}

void ManifestSnapshot() {
  Fixture fixture;
  const bool opened = fixture.Open(true);
  CHECK(opened, "actual native road page dependencies prepare before manifest contention");
  if (!opened) { return; }
  const auto expected = Generators::EncodeRoadPageManifest(
      *fixture.Manifest, Generators::PreparedRoadPages::ManifestBytesMost);
  auto write = fixture.Pages->PrepareManifest(*fixture.Manifest);
  CHECK(write && expected, "manifest bytes and dependency metadata are prepared together");
  if (!write || !expected) { return; }
  const auto key = write->Key();
  const auto bytes = write->PublicationBytes();
  fixture.Manifest->Pages.clear();
  const std::weak_ptr<const Generators::RoadPageManifest> original = fixture.Manifest;
  fixture.Manifest.reset();
  CHECK(original.expired(),
        "manifest admission owns a small dependency snapshot, not borrowed material/source input");
  fixture.Hold();
  for (unsigned attempt = 0; attempt < 3; ++attempt) {
    const auto pending = fixture.Pages->StoreManifest(*write);
    CHECK(!pending && Busy(pending.error()) && write->PublicationBytes() == bytes,
          "manifest Busy retains encoded bytes and its original dependency selection after source "
          "release");
  }
  fixture.Release();
  const auto stored = fixture.Pages->StoreManifest(*write);
  const auto restored = fixture.Pages->LoadManifest(key);
  CHECK(stored && *stored == key && restored && *restored && (**restored)->Pages.size() == 1 &&
            Generators::EncodeRoadPageManifest(
                ***restored, Generators::PreparedRoadPages::ManifestBytesMost) == expected,
        "the original native manifest is validated after commit without re-encoding source input");
}

void PermanentAndCancel() {
  Fixture fixture;
  const bool opened = fixture.Open();
  CHECK(opened, "the native failure fixture opens");
  if (!opened) { return; }
  auto write = fixture.Cache->PrepareStore(fixture.Key,
                                           GroundRegionBoundsEcef(*fixture.Region),
                                           *fixture.Region,
                                           *fixture.Water,
                                           fixture.Request);
  CHECK(write, "valid native bytes exist before the storage constraint is introduced");
  if (!write) { return; }
  CHECK(sqlite3_exec(
            fixture.Writer.get(),
            "CREATE TRIGGER reject_region BEFORE INSERT ON assets WHEN NEW.kind = 'ground-region' "
            "BEGIN SELECT RAISE(ABORT, 'fixture permanent failure'); END",
            nullptr,
            nullptr,
            nullptr) == SQLITE_OK,
        "a real permanent SQLite constraint rejects region publication");
  const auto failed = fixture.Cache->Store(*write);
  CHECK(!failed && !Busy(failed.error()) && write->PublicationBytes() == 0 &&
            write->PublicationAttempts() == 1,
        "permanent errors terminate after one attempt and release pending native ownership");
  CHECK(
      sqlite3_exec(fixture.Writer.get(), "DROP TRIGGER reject_region", nullptr, nullptr, nullptr) ==
          SQLITE_OK,
      "the independent fixture removes only its own failure constraint");
  auto canceled = fixture.Cache->PrepareStore(fixture.Key,
                                              GroundRegionBoundsEcef(*fixture.Region),
                                              *fixture.Region,
                                              *fixture.Water,
                                              fixture.Request);
  CHECK(canceled, "a separate pending publication can be canceled");
  if (!canceled) { return; }
  std::stop_source stop;
  stop.request_stop();
  const auto result = fixture.Cache->Store(*canceled, stop.get_token());
  CHECK(!result && canceled->PublicationBytes() == 0 && canceled->PublicationAttempts() == 0,
        "cancellation drops prepared bytes before any storage attempt");
}
}

int main() {
  ColdThenWarmBind();
  ManifestSnapshot();
  PermanentAndCancel();
  return Report();
}
