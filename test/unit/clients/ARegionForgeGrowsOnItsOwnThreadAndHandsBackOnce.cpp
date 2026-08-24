#include <atomic>
#include <cmath>
#include <cstdio>
#include <memory>
#include <optional>
#include <vector>

#include "Check.h"

#include "ClassStructure.h"
#include "FeatureField.h"
#include "Generator.h"
#include "GeneratorSet.h"
#include "Ground.h"
#include "GroundPatch.h"
#include "GroundTable.h"
#include "Rank.h"
#include "Region.h"
#include "RegionForge.h"
#include "RegionPool.h"
#include "Yield.h"

using outshine::Generators::Rank;
using outshine::Span;
using outshine::TangentFrame;
using outshine::ClassStructure;
using outshine::Clients::RegionForge;
using outshine::Generators::Body;
using outshine::Generators::FeatureField;
using outshine::Generators::Generator;
using outshine::Generators::GeneratorSet;
using outshine::Generators::Ground;
using outshine::Generators::GroundPatch;
using outshine::Generators::GroundTable;
using outshine::Generators::Region;
using outshine::Generators::RegionPool;
using outshine::Generators::Yield;

namespace {

// board:1806: RegionForge was drawn in the CURRENT class map and named by nothing under test/.
// It is the tree's one worker thread outside the tile pool: a region goes in, a generator set
// runs over it away from the frame, and a grown region comes back. What it owes is a state
// machine -- idle, growing, done -- and a hand-back that happens exactly once.
constexpr int kSide = 5;

[[nodiscard]] Ground::Snapshot Flat(const Region &region) {
  std::vector<GroundPatch::Posting> postings((size_t)kSide * kSide);
  for (auto &posting : postings) { posting.Height = outshine::GroundSample::At(400.0); }

  auto fine = std::make_shared<ClassStructure::Grid>();
  fine->W = fine->H = 1;
  fine->OrgE = fine->OrgN = -100.0;
  fine->CellM = 4000.0;
  fine->Cells = {0u, 0u};
  fine->Seeds = {0u, 0u, 0u};
  fine->Refs = {0, 1, 2, 3, 4, 5, 6, 7};

  std::vector<GroundTable::Row> rows(1);
  rows[0].SlopeMaxDeg = 60.0f;

  Ground::Snapshot out;
  out.Patch = GroundPatch::Complete(
      region, kSide, Span<const GroundPatch::Posting>(postings.data(), postings.size()));
  out.Classes = std::make_shared<const ClassStructure>(
      TangentFrame::At(region.AnchorLat(), region.AnchorLon()), fine, fine, 1u, 0, 0.0, 0);
  out.Features = FeatureField::Of(Span<const FeatureField::Feature>(),
                                  Span<const FeatureField::Ring>(),
                                  Span<const FeatureField::Vertex>());
  out.Table = GroundTable::Of(Span<const GroundTable::Row>(rows.data(), rows.size()));
  return out;
}

// a generator that places a known number of bodies and counts how often it was asked, so the
// twin can tell one growth from two.
class Counted final : public Generator {
public:
  explicit Counted(uint32_t places) : Places_(places) {}

  void Occupy(const Ground &ground, Yield &yield) const noexcept override {
    ++Ran_;
    for (uint32_t at = 0; at < Places_; ++at) {
      Body body;
      body.Em = (double)at;
      body.Nm = 0.0;
      body.BaseAslM = ground.HeightAslM(0.0, 0.0);
      body.RadiusM = 0.5f;
      body.HeightM = 2.0f;
      (void)yield.Place(body);
    }
  }
  [[nodiscard]] uint32_t Proposes(double areaM2) const noexcept override {
    (void)areaM2;
    return Places_;
  }
  [[nodiscard]] bool At(const Ground &ground, double eastM, double northM,
                        Body *out) const noexcept override {
    (void)ground;
    (void)eastM;
    (void)northM;
    (void)out;
    return false;
  }
  [[nodiscard]] long Ran() const { return Ran_.load(); }

private:
  uint32_t Places_;
  mutable std::atomic<long> Ran_{0};
};

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const Region region = Region::Of(14, 48.137, 11.575);
  const std::optional<Ground> ground = Ground::Of(region, Flat(region));
  CHECK(ground.has_value(), "a synthetic ground stands up from a complete snapshot");
  if (!ground) { return Report(); }

  RegionPool::Extent extent{region, region};
  RegionPool::Shape shape;
  shape.BodyCapacity = 64;
  RegionPool pool(extent, shape);

  const Counted places(7);
  GeneratorSet generators;
  CHECK(generators.Add(Rank{0}, places), "one generator joins the set");
  Note("generators in the set", (double)generators.Count(), "generators");

  RegionForge forge(generators);
  CHECK(forge.Idle() && !forge.UnderWay().has_value(),
        "**A FORGE THAT WAS ASKED NOTHING IS IDLE AND NAMES NO REGION**: the state is readable "
        "before the first order, so a caller can tell 'not started' from 'started and silent' "
        "(board:1806)");
  CHECK(!forge.Collect().has_value(),
        "and collecting from it hands back nothing rather than an empty region");

  std::optional<RegionPool::Lease> lease = pool.TryAcquire(*ground);
  CHECK(lease.has_value(), "the pool leases space for the region");
  if (!lease) { return Report(); }

  forge.Request(*ground, std::move(*lease));

  // the forge names what it is growing while it grows, and stops being idle the moment it is
  // asked -- that is what makes a caller able to cancel the right region.
  std::optional<RegionForge::Grown> grown;
  long polls = 0;
  bool namedItsRegion = false;
  for (; polls < 200000 && !grown; ++polls) {
    const std::optional<Region> under = forge.UnderWay();
    namedItsRegion = namedItsRegion || (under.has_value() && under->Is(region));
    grown = forge.Collect();
  }
  Note("polls before it handed back", (double)polls, "polls");
  Note("bodies it grew", grown ? (double)grown->Space.Sink().Placed().Size() : -1.0, "bodies");
  Note("times the generator ran", (double)places.Ran(), "times");
  Note("how long it said it took", grown ? grown->OccupyMs : -1.0, "ms");

  CHECK(grown.has_value(),
        "**AND A REQUESTED REGION COMES BACK GROWN**: the work happens on the forge's own "
        "thread and the caller polls rather than blocks, which is what keeps a frame free of "
        "a generator set");
  if (!grown) { return Report(); }
  CHECK(namedItsRegion || polls <= 1,
        "and while it worked it named the region it was working on, so a caller that changes "
        "its mind cancels the right one");
  CHECK(grown->Space.Sink().Placed().Size() == 7 && places.Ran() == 1,
        "with the seven bodies the generator places, grown exactly once");
  CHECK(grown->Where.Where().Is(region),
        "and the ground it hands back is the ground it was handed");

  CHECK(!forge.Collect().has_value(),
        "**AND A GROWN REGION IS HANDED BACK ONCE**: a second collect returns nothing, so two "
        "callers cannot both take the same lease and the pool cannot be double-freed");
  CHECK(forge.Idle(), "and the forge is idle again, ready for the next order");

  // cancelling an idle forge is not an error, and a cancelled order does not come back.
  forge.Cancel();
  CHECK(forge.Idle() && !forge.Collect().has_value(),
        "cancelling a forge that is doing nothing leaves it doing nothing");

  Covers("I.21.3 the region forge grows on its own thread: it is idle before its first order, "
         "names the region while it works, hands the grown region back exactly once, and is "
         "idle again after (board:1806)");
  return Report();
}
