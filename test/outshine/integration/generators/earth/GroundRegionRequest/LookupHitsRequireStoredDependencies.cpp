#include "OpenAssetStore.h"
#include "test/harness/generators/GroundPublicationFixture.h"
#include "StoreGroundRegion.h"
#include "GroundRegionAsset.h"
#include "PreparedRoadPages.h"
#include "Check.h"
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Test;
using namespace GroundPublication;

void MissingRequest() {
  Fixture fixture;
  const bool opened = fixture.Open();
  CHECK(opened, "an empty native asset database opens without source inputs");
  if (!opened) { return; }
  fixture.Job = std::make_unique<Generators::GroundRegionRequest>(
      fixture.Pool, Generators::GroundRegionRequest::Lookup(fixture.Cache, fixture.Request));
  const auto result = fixture.Await();
  CHECK(result && *result, "a missing request completes as a miss rather than an error");
  if (!result || !*result) { return; }
  const auto &value = result->value();
  CHECK(!value.RequestHit && !value.Loaded && !value.SurfacePages && value.Key.empty() &&
            value.RequestKey == fixture.Request,
        "an empty source-free probe cannot report a request cache hit");
}

void StoredRequest(bool roads, bool manifestStored) {
  Fixture fixture;
  const bool opened = fixture.Open(roads);
  CHECK(opened, "native region and optional road inputs prepare without network access");
  if (!opened) { return; }
  if (roads && manifestStored) {
    auto write = fixture.Pages->PrepareManifest(*fixture.Manifest);
    CHECK(write, "the complete-hit control prepares its actual road manifest");
    if (!write) { return; }
    const auto stored = fixture.Pages->StoreManifest(*write);
    CHECK(stored, "the complete-hit control persists its actual road manifest");
    if (!stored) { return; }
  }
  const auto stored = StoreGroundRegion(*fixture.Cache,
                                        fixture.Key,
                                        GroundRegionBoundsEcef(*fixture.Region),
                                        *fixture.Region,
                                        *fixture.Water,
                                        fixture.Request);
  CHECK(stored, "a real native region and its request binding persist independently of lookup");
  if (!stored) { return; }
  auto fresh = Regions::Open(OpenAssetStore(fixture.Directory), fixture.Sources, "fixture-rules");
  CHECK(fresh, "a fresh cache owner restores the persisted request without generation inputs");
  if (!fresh) { return; }
  fixture.Job = std::make_unique<Generators::GroundRegionRequest>(
      fixture.Pool,
      Generators::GroundRegionRequest::Lookup(std::move(*fresh), fixture.Request),
      fixture.Pages);
  const auto result = fixture.Await();
  CHECK(result && *result, "native dependency restoration completes without source work");
  if (!result || !*result) { return; }
  const auto &value = result->value();
  CHECK(value.Key == fixture.Key && value.RequestKey == fixture.Request,
        "the stored region and logical request identities survive dependency restoration");
  if (roads && !manifestStored) {
    CHECK(!value.Loaded && !value.SurfacePages && !value.RequestHit,
          "a missing required road manifest invalidates both the region and its hit status");
  } else {
    CHECK(value.Loaded && value.RequestHit && bool(value.SurfacePages) == roads,
          "a complete stored product remains a true hit with its optional road dependencies");
  }
}
}

int main() {
  MissingRequest();
  StoredRequest(false, false);
  StoredRequest(true, false);
  StoredRequest(true, true);
  return Report();
}
