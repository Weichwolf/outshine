#include "test/harness/generators/GroundPublicationFixture.h"
#include "StoreGroundRegion.h"
#include "Sha256.h"
#include "Check.h"
#include <optional>
#include <memory>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Test;
using namespace GroundPublication;
using Request = Generators::GroundRegionRequest;

void Exercise() {
  Fixture fixture;
  CHECK(fixture.Open(), "native region source-identity fixture opens");
  if (!fixture.Cache) { return; }
  const auto stored = StoreGroundRegion(*fixture.Cache,
                                        fixture.Key,
                                        GroundRegionBoundsEcef(*fixture.Region),
                                        *fixture.Region,
                                        *fixture.Water,
                                        fixture.Request);
  CHECK(stored, "the canonical native product is bound before any source exists");
  if (!stored) { return; }
  fixture.Job = std::make_unique<Request>(
      fixture.Pool, Request::Lookup(fixture.Cache, fixture.Request, Request::IdentityInputs{}));
  const auto hit = fixture.Await();
  CHECK(hit && *hit && hit->value().Loaded && hit->value().RequestHit &&
            hit->value().Key == fixture.Key,
        "a stored request resolves without examining unavailable source identity inputs");
  fixture.Job = std::make_unique<Request>(fixture.Pool,
                                          Request::Lookup(fixture.Cache,
                                                          Sha256Hex("missing native region"),
                                                          Request::IdentityInputs{}));
  const auto miss = fixture.Await();
  CHECK(miss && !*miss && miss->error() == "ground region contains unbound vector inputs",
        "a miss rejects unbound source identity instead of publishing a false hit");
}
}

int main() {
  Exercise();
  return outshine::Test::Report();
}
