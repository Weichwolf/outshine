/* THE GATE THAT RUNS. Everything else under this directory is answered by the compiler; that a
 * region is a function of place is only answered by generating one twice and comparing the bytes.
 * The second pass reuses a sink that has meanwhile held another region, so state surviving an Open
 * fails here rather than as a forest that moves when the viewer walks a ring.
 *
 * The class is checked the same way: the grid below has two overlapping outlines and nothing else,
 * so what a candidate classifies as is known in closed form at every point — and a lattice would
 * have to answer with the nearest of its postings. */
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <optional>
#include <vector>

#include "ClassStructure.h"
#include "Generator.h"
#include "GeneratorSet.h"
#include "Ground.h"
#include "GroundPatch.h"
#include "GroundTable.h"
#include "Rank.h"
#include "Region.h"
#include "RegionPool.h"
#include "Schedule.h"
#include "SurfaceState.h"
#include "Yield.h"

using namespace outshine;
using namespace outshine::Generators;

namespace {

/* "The hot path allocates nothing" is a claim about a signature until something counts. */
long gAllocations = 0;
bool gCounting = false;

int gFailures = 0;

void Check(bool ok, const char *what) {
  if (ok) return;
  std::printf("FAIL %s\n", what);
  gFailures++;
}

uint64_t Mix(uint64_t v) {
  v += 0x9e3779b97f4a7c15ull;
  v = (v ^ (v >> 30)) * 0xbf58476d1ce4e5b9ull;
  v = (v ^ (v >> 27)) * 0x94d049bb133111ebull;
  return v ^ (v >> 31);
}

double Unit(uint64_t v) { return (double)(v >> 11) * (1.0 / 9007199254740992.0); }

constexpr int kSide = 33;
constexpr double kCellM = 8.0;
constexpr uint32_t kBodies = 8192;

/* THE TWO OUTLINES, in the class structure's own metres: a wood of the higher rank and a scrub of
 * the lower, overlapping over 300 m. Everything outside both is the coarse grid's meadow. */
constexpr double kWoodW = 200.0, kWoodE = 900.0;
constexpr double kScrubW = 600.0, kScrubE = 1200.0;
constexpr int kMeadowRow = 0, kWoodRow = 1, kScrubRow = 2;

struct Truth {
  int Row;
  double EdgeM;
  bool HasEdge;
  int RunnerUp;
};

Truth Expected(double classEm) {
  if (classEm >= kWoodW && classEm < kWoodE)
    return Truth{kWoodRow, std::min(classEm - kWoodW, kWoodE - classEm), true,
                 classEm >= kScrubW ? kScrubRow : -1};
  if (classEm >= kScrubW && classEm < kScrubE)
    return Truth{kScrubRow, std::min(classEm - kScrubW, kScrubE - classEm), true, -1};
  return Truth{kMeadowRow, 0.0, false, -1};
}

void PushEdges(ClassStructure::Grid &g, double west, double east) {
  /* The outline runs a kilometre past the region on both sides, so the nearest edge to any sample
   * is one of the two that matter and the expected distance stays closed-form. */
  const float ring[4][4] = {{(float)west, -1000.0f, (float)east, -1000.0f},
                            {(float)east, -1000.0f, (float)east, 5000.0f},
                            {(float)east, 5000.0f, (float)west, 5000.0f},
                            {(float)west, 5000.0f, (float)west, -1000.0f}};
  for (const auto &e : ring)
    for (float v : e) g.Edges.push_back(v);
}

/* One acceleration cell holding both outlines: the same words the fragment reads, laid down by
 * hand because ClassBuilder is world/ and has no spelling here. */
std::shared_ptr<const ClassStructure> SyntheticClasses(const TangentFrame &frame) {
  auto fine = std::make_shared<ClassStructure::Grid>();
  fine->W = fine->H = 1;
  fine->OrgE = fine->OrgN = -100.0;
  fine->CellM = 4000.0;
  fine->Cells = {0xFFu | (2u << 16), 0u};   /* no base class, two seeds */
  const uint32_t wind = 128u;              /* the cell corner lies outside both outlines */
  fine->Seeds = {(uint32_t)kWoodRow | (5u << 8) | (4u << 16) | (wind << 24), 0u, 0u,
                 (uint32_t)kScrubRow | (3u << 8) | (4u << 16) | (wind << 24), 4u, 0u};
  fine->Refs = {0, 1, 2, 3, 4, 5, 6, 7};
  PushEdges(*fine, kWoodW, kWoodE);
  PushEdges(*fine, kScrubW, kScrubE);

  auto coarse = std::make_shared<ClassStructure::Grid>();
  coarse->W = coarse->H = 1;
  coarse->OrgE = coarse->OrgN = -2000.0;
  coarse->CellM = 8000.0;
  coarse->Cells = {(uint32_t)kMeadowRow | (1u << 8), 0u};
  return std::make_shared<const ClassStructure>(frame, fine, coarse, 1, kMeadowRow, 0.0, 0);
}

Ground::Snapshot SyntheticSnapshot(const Region &region) {
  std::vector<GroundPatch::Posting> postings((size_t)kSide * kSide);
  for (int j = 0; j < kSide; j++) {
    for (int i = 0; i < kSide; i++) {
      GroundPatch::Posting &p = postings[(size_t)j * kSide + (size_t)i];
      const double e = (double)i / (kSide - 1), n = (double)j / (kSide - 1);
      p.Height = GroundSample::At(400.0 + 120.0 * e + 40.0 * std::sin(12.0 * n));
      p.HasWater = e > 0.85;
      p.WaterLevelAslM = 505.0f;
    }
  }
  std::vector<GroundTable::Row> rows(3);
  rows[0].SlopeMaxDeg = 35.0f;
  rows[1].SlopeMaxDeg = 60.0f;
  rows[2].SlopeMaxDeg = 5.0f;
  rows[1].Surface.Coverage = 0.4f;
  Ground::Snapshot s;
  s.Patch = GroundPatch::Complete(region, kSide, Span<const GroundPatch::Posting>(
                                                     postings.data(), postings.size()));
  s.Classes = SyntheticClasses(TangentFrame::At(region.AnchorLat(), region.AnchorLon()));
  s.Features = FeatureField::Of(Span<const FeatureField::Feature>(),
                                Span<const FeatureField::Ring>(),
                                Span<const FeatureField::Vertex>());
  s.Table = GroundTable::Of(Span<const GroundTable::Row>(rows.data(), rows.size()));
  return s;
}

/* A stand-in for the generators step 6 onwards moves in: a jittered lattice, the ground's own rules,
 * and a trunk that claims its space. */
class Scatter : public Generator {
public:
  enum Note { NoClass, TooSteep, InWater, Occupied, HighestStandAslM, kNotes };

  explicit Scatter(double stepM) : StepM_(stepM) {}

  Span<const char *const> NoteNames() const noexcept override {
    static const char *const names[kNotes] = {"noClass", "tooSteep", "inWater", "occupied",
                                              "highestStandAslM"};
    return Span<const char *const>(names, kNotes);
  }

  void Occupy(const Ground &ground, Yield &yield) const noexcept override {
    const Region &region = ground.Where();
    const int cols = (int)(region.SpanEm() / StepM_), rows = (int)(region.SpanNm() / StepM_);
    for (int j = 0; j < rows; j++) {
      for (int i = 0; i < cols; i++) {
        const uint64_t seed = region.Seed((uint64_t)j * 4096u + (uint64_t)i);
        const double e = ((double)i + Unit(seed)) * StepM_;
        const double n = ((double)j + Unit(Mix(seed))) * StepM_;
        Body body;
        if (!At(ground, e, n, &body)) continue;
        switch (yield.Place(body).Why()) {
          case Claim::Outcome::Placed: yield.Raise(HighestStandAslM, body.BaseAslM); break;
          case Claim::Outcome::Occupied: yield.Count(Occupied); break;
          case Claim::Outcome::Outside: break;
          case Claim::Outcome::Full: return;
        }
      }
    }
  }

  bool At(const Ground &ground, double eastM, double northM, Body *out) const noexcept override {
    int row = 0;
    const Cover cover = ground.CoverAt(eastM, northM);
    if (!cover.TryRow(&row)) return false;
    if (ground.SlopeDeg(eastM, northM) > (double)ground.Table().At((size_t)row).SlopeMaxDeg)
      return false;
    double level = 0.0;
    if (ground.TryWaterLevelAslM(eastM, northM, &level)) return false;
    const uint64_t seed = Mix((uint64_t)(eastM * 64.0) ^ (Mix((uint64_t)(northM * 64.0)) << 1));
    out->Em = eastM;
    out->Nm = northM;
    out->BaseAslM = ground.HeightAslM(eastM, northM);
    out->RadiusM = 0.4f + 12.0f * (float)Unit(seed);
    out->HeightM = 18.0f + 14.0f * (float)Unit(Mix(seed));
    out->MassKg = 2200.0f;
    out->YawRad = (float)(Unit(Mix(seed + 7u)) * 6.283185307179586);
    out->Contact = ContactMaterial{1};
    return true;
  }

private:
  double StepM_;
};

/* One yield per registered generator, over the storage the caller brought: what step 6's region job
 * does at bring-up, done here so that the gate exercises the same shape. */
std::vector<Yield> Yields(const GeneratorSet &set, OccupancySink &sink,
                          std::vector<Yield::Note> &notes) {
  std::vector<Yield> yields;
  size_t at = 0;
  for (size_t i = 0; i < set.Count(); i++) {
    const Span<const char *const> names = set.At(i).NoteNames();
    yields.push_back(Yield(sink, names, Span<Yield::Note>(notes.data() + at, names.Size())));
    at += names.Size();
  }
  return yields;
}

struct Run {
  std::vector<Body> Bodies;
  std::vector<BodyRange> Ranges;
  std::vector<uint32_t> Conflicts;   /* per yield, against the sink's own total */
  uint32_t Occupied = 0, Outside = 0, Full = 0;
  double OccupyMs = 0.0;
};

/* Acquires, fills and gives the buffers straight back, so the next region gets a sink that has just
 * held another one — which is the only way a leftover shows up as a moved body. */
Run Filled(RegionPool &pool, const Ground &ground, const GeneratorSet &set,
           std::vector<Yield::Note> &notes) {
  std::optional<RegionPool::Lease> lease = pool.TryAcquire(ground);
  if (!lease) return Run();
  OccupancySink &sink = lease->Sink();
  std::vector<Yield> yields = Yields(set, sink, notes);
  const auto started = std::chrono::steady_clock::now();
  gCounting = true;
  set.Occupy(ground, Span<Yield>(yields.data(), yields.size()));
  gCounting = false;
  Run run;
  run.OccupyMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
  run.Bodies.assign(sink.Placed().begin(), sink.Placed().end());
  for (const Yield &y : yields) {
    run.Ranges.push_back(y.Placed());
    run.Conflicts.push_back(y.Claims(Claim::Outcome::Occupied));
  }
  run.Occupied = sink.Claims(Claim::Outcome::Occupied);
  run.Outside = sink.Claims(Claim::Outcome::Outside);
  run.Full = sink.Claims(Claim::Outcome::Full);
  return run;
}

bool Same(const Yield::Note &a, const Yield::Note &b) {
  return a.Name == b.Name && a.Times == b.Times && a.Raised == b.Raised &&
         (!a.Raised || a.Peak == b.Peak);
}

} // namespace

void *operator new(size_t bytes) {
  if (gCounting) gAllocations++;
  void *block = std::malloc(bytes ? bytes : 1);
  if (!block) std::abort();
  return block;
}
void *operator new[](size_t bytes) { return operator new(bytes); }
void operator delete(void *block) noexcept { std::free(block); }
void operator delete[](void *block) noexcept { std::free(block); }
void operator delete(void *block, size_t) noexcept { std::free(block); }
void operator delete[](void *block, size_t) noexcept { std::free(block); }

int main() {
  const Region a(14, 8548, 5626), b(14, 8549, 5626);
  Check(a.Seed() != b.Seed(), "two regions share a seed");
  Check(Region::Of(14, 51.96, 9.55).Is(Region::Of(14, 51.96, 9.55)), "Region::Of is not a function");

  double lat = 0.0, lon = 0.0, e = 0.0, n = 0.0;
  a.Geo(1234.5, 678.9, &lat, &lon);
  a.Enu(lat, lon, &e, &n);
  Check(std::fabs(e - 1234.5) < 1e-6 && std::fabs(n - 678.9) < 1e-6, "Enu/Geo do not round-trip");

  const Ground::Snapshot snapA = SyntheticSnapshot(a), snapB = SyntheticSnapshot(b);
  const std::optional<Ground> groundA = Ground::Of(a, snapA), groundB = Ground::Of(b, snapB);
  Check(groundA.has_value() && groundB.has_value(), "a complete snapshot yielded no Ground");
  Ground::Snapshot classless = snapA;
  classless.Classes.reset();
  Check(!Ground::Of(a, classless), "a Ground was offered with no classifier");

  /* WHAT THE TWO FRAMES COST EACH OTHER: the region measures east with the cosine at the sample and
   * the class structure with an ECEF tangent plane, so the same place has two sets of metres. The
   * conversion runs through geodetic coordinates; this is what skipping it would have been worth. */
  double frameOffsetM = 0.0;
  for (int j = 0; j <= 8; j++)
    for (int i = 0; i <= 8; i++) {
      const double em = a.SpanEm() * (double)i / 8.0, nm = a.SpanNm() * (double)j / 8.0;
      a.Geo(em, nm, &lat, &lon);
      snapA.Classes->Frame().Project(lat, lon, &e, &n);
      frameOffsetM = std::max(frameOffsetM, std::max(std::fabs(e - em), std::fabs(n - nm)));
    }

  /* THE CLASS IS A FUNCTION: every sample answers what the outlines say at that very point, and the
   * finest boundary it resolves is bounded by nothing but the sample spacing. */
  constexpr int kProbeSide = 200;
  std::vector<Truth> want((size_t)kProbeSide * kProbeSide);
  std::vector<Cover> got((size_t)kProbeSide * kProbeSide);
  double finestEdgeM = 1.0e30;
  for (int j = 0; j < kProbeSide; j++)
    for (int i = 0; i < kProbeSide; i++) {
      const double em = a.SpanEm() * ((double)i + 0.5) / kProbeSide;
      const double nm = a.SpanNm() * ((double)j + 0.5) / kProbeSide;
      a.Geo(em, nm, &lat, &lon);
      snapA.Classes->Frame().Project(lat, lon, &e, &n);
      Truth &t = want[(size_t)j * kProbeSide + (size_t)i];
      t = Expected(e);
      if (t.HasEdge) finestEdgeM = std::min(finestEdgeM, t.EdgeM);
    }
  /* What one candidate costs the generator: the whole hop, region metres to a class row. */
  const auto probesStarted = std::chrono::steady_clock::now();
  for (int j = 0; j < kProbeSide; j++)
    for (int i = 0; i < kProbeSide; i++)
      got[(size_t)j * kProbeSide + (size_t)i] =
          groundA->CoverAt(a.SpanEm() * ((double)i + 0.5) / kProbeSide,
                           a.SpanNm() * ((double)j + 0.5) / kProbeSide);
  const double coverNs =
      std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - probesStarted)
          .count() /
      (double)got.size();

  long classProbes = 0, classWrong = 0, edgeWrong = 0, runnerUpWrong = 0;
  for (size_t i = 0; i < got.size(); i++) {
    int row = -1, runnerUp = -1;
    float edgeM = 0.0f;
    classProbes++;
    if (!got[i].TryRow(&row) || row != want[i].Row) classWrong++;
    if (got[i].TryEdgeM(&edgeM) != want[i].HasEdge ||
        (want[i].HasEdge && std::fabs((double)edgeM - want[i].EdgeM) > 0.05))
      edgeWrong++;
    if (got[i].TryRunnerUp(&runnerUp) != (want[i].RunnerUp >= 0) ||
        (want[i].RunnerUp >= 0 && runnerUp != want[i].RunnerUp))
      runnerUpWrong++;
  }
  Check(classWrong == 0, "the class disagreed with the outlines it came from");
  Check(edgeWrong == 0, "the distance to the wood line disagreed with the wood line");
  Check(runnerUpWrong == 0, "the runner-up disagreed with the outline under the winner");

  std::vector<GroundPatch::Posting> holed((size_t)kSide * kSide);
  holed[17].Height = GroundSample::Missing();
  for (size_t i = 0; i < holed.size(); i++)
    if (i != 17) holed[i].Height = GroundSample::At(300.0);
  Check(!GroundPatch::Complete(a, kSide, Span<const GroundPatch::Posting>(holed.data(),
                                                                         holed.size())),
        "a patch with a hole in it was offered");

  const FeatureField::Vertex verts[4] = {{10.0f, 20.0f}, {40.0f, 20.0f}, {40.0f, 60.0f},
                                         {10.0f, 60.0f}};
  const FeatureField::Ring rings[1] = {{0, 4}};
  FeatureField::Feature feats[1] = {{0, 1, 7, 0.0f, 0.0f, 0.0f, 0.0f}};
  const std::shared_ptr<const FeatureField> field =
      FeatureField::Of(Span<const FeatureField::Feature>(feats, 1),
                       Span<const FeatureField::Ring>(rings, 1),
                       Span<const FeatureField::Vertex>(verts, 4));
  Check(field && field->At(0).MinEm == 10.0f && field->At(0).MaxEm == 40.0f &&
            field->At(0).MinNm == 20.0f && field->At(0).MaxNm == 60.0f,
        "the feature box was not derived from the rings");
  Check(field && !FeatureField::Boxed(field->At(0), 41.0, 30.0) &&
            FeatureField::Boxed(field->At(0), 20.0, 30.0),
        "the feature box holds the wrong points");
  Check(field && field->Contains(field->At(0), 20.0, 30.0) &&
            !field->Contains(field->At(0), 200.0, 30.0),
        "the outline holds the wrong points");

  /* TWO of them, because one generator cannot show that the second one's bodies start where the
   * first one's end — and a set of one is not the contract. */
  const Scatter scatter(30.0), sparse(97.0);
  GeneratorSet set;
  Check(set.Add(Rank{10}, scatter), "the first registration was refused");
  Check(!set.Add(Rank{10}, scatter), "a duplicate rank was accepted");
  Check(set.Add(Rank{20}, sparse), "the second registration was refused");
  const size_t noteCount = 2 * Scatter::kNotes;
  std::vector<Yield::Note> notes(noteCount), notesB(noteCount), notes2(noteCount),
      notes3(noteCount);

  const Schedule schedule(Schedule::Ring{14, 2});
  RegionPool pool(schedule, RegionPool::Shape{2, kBodies, kCellM});
  {
    std::optional<RegionPool::Lease> first = pool.TryAcquire(*groundA);
    std::optional<RegionPool::Lease> second = pool.TryAcquire(*groundA);
    Check(first && second && !pool.TryAcquire(*groundA),
          "the pool handed out more sinks than it holds");
    second.reset();
    Check(pool.Free() == 1, "a released lease did not come back");
  }
  Check(pool.Free() == 2, "the pool did not come back whole");

  const Run run1 = Filled(pool, *groundA, set, notes);
  const Run runB = Filled(pool, *groundB, set, notesB);
  const Run run2 = Filled(pool, *groundA, set, notes2);
  /* Held, so the third pass runs in the OTHER buffer set. */
  std::optional<RegionPool::Lease> held = pool.TryAcquire(*groundA);
  const Run run3 = Filled(pool, *groundA, set, notes3);
  held.reset();

  Check(!run1.Bodies.empty(), "nothing was placed at all");
  Check(run1.Occupied > 0, "nothing was ever refused, so the claim was never tested");
  Check(run1.Outside == 0 && run1.Full == 0, "the scatter left its own region or ran out of room");
  Check(run1.Bodies.size() == run2.Bodies.size() && run1.Bodies.size() == run3.Bodies.size(),
        "the count moved");
  Check(run1.Bodies.size() == run2.Bodies.size() &&
            std::memcmp(run1.Bodies.data(), run2.Bodies.data(),
                        run1.Bodies.size() * sizeof(Body)) == 0,
        "the same region in the same sink placed different bytes");
  Check(run1.Bodies.size() == run3.Bodies.size() &&
            std::memcmp(run1.Bodies.data(), run3.Bodies.data(),
                        run1.Bodies.size() * sizeof(Body)) == 0,
        "the same region in another sink placed different bytes");
  Check(run1.Occupied == run2.Occupied && run1.Occupied == run3.Occupied, "the refusals moved");
  Check(run1.Bodies.size() != runB.Bodies.size() ||
            std::memcmp(run1.Bodies.data(), runB.Bodies.data(),
                        run1.Bodies.size() * sizeof(Body)) != 0,
        "two different regions placed the same thing");

  bool sameNotes = true;
  for (size_t i = 0; i < notes.size(); i++) sameNotes = sameNotes && Same(notes[i], notes2[i]);
  Check(sameNotes, "the same region kept different notes");
  Check(run1.Ranges.size() == 2 && run1.Ranges[0].First == 0 &&
            run1.Ranges[1].First == run1.Ranges[0].Count &&
            run1.Ranges[0].Count + run1.Ranges[1].Count == run1.Bodies.size(),
        "the two generators' stretches of the sink do not tile what was placed");
  Check(run1.Ranges.size() == 2 && run1.Ranges[1].Count > 0,
        "the second generator placed nothing, so its stretch proves nothing");
  Check(notes[Scatter::HighestStandAslM].Raised && notes[Scatter::HighestStandAslM].Peak > 400.0,
        "the generator's high-water did not land");
  Check(run1.Conflicts.size() == 2 && notes[Scatter::Occupied].Times == run1.Conflicts[0] &&
            run1.Conflicts[0] + run1.Conflicts[1] == run1.Occupied,
        "the yields do not partition the sink's refusals");
  Check(std::strcmp(notes[Scatter::InWater].Name, "inWater") == 0,
        "a note lost the name its generator declared");

  const std::optional<BodyId> id = run1.Bodies[0].Id();
  Check(id && id->Index() == 0, "a placed body carries no id");
  Check(!Body().Id(), "an unplaced body carries an id");

  {
    std::optional<RegionPool::Lease> lease = pool.TryAcquire(*groundA);
    Body outside = run1.Bodies[0];
    outside.Em = a.SpanEm() + 1.0;
    Check(lease->Sink().Place(outside).Why() == Claim::Outcome::Outside,
          "a body outside the region was not refused as outside");
    BodyId placed = *run1.Bodies[0].Id();
    Check(lease->Sink().Place(run1.Bodies[0]).TryId(&placed) &&
              placed.Index() == 0,
          "the first body of an open sink was not claimed");
    Check(lease->Sink().Place(run1.Bodies[0]).Why() == Claim::Outcome::Occupied,
          "a body inside one already standing was not refused as occupied");
  }

  /* THE FOURTH OUTCOME, which a sink sized for a region never reaches. */
  RegionPool tiny(schedule, RegionPool::Shape{1, 4, kCellM});
  std::vector<Yield::Note> tinyNotes(noteCount);
  const Run cramped = Filled(tiny, *groundA, set, tinyNotes);
  Check(cramped.Bodies.size() == 4 && cramped.Full == 2,
        "a full sink did not say so once to each generator");

  Check(schedule.Count() == 25, "the ring is not (2R+1)^2");
  const std::optional<Region> nearest = schedule.At(0, 51.96, 9.55);
  Check(nearest && nearest->Is(Region::Of(14, 51.96, 9.55)), "the nearest region is not the centre");

  constexpr Material leaf{{0.1f, 0.3f, 0.1f}, 0.6f, 0.35f, 0.4f, 1.0f, {0.0f, 0.0f, 0.0f}};
  constexpr Material glass{{0.0f, 0.0f, 0.0f}, 0.05f, 1.0f, 0.9f, 1.5f, {0.0f, 0.0f, 0.0f}};
  constexpr Material bark{};
  static_assert(StateOf(leaf).Kind() == SurfaceKind::ThinTransmissive, "leaf");
  static_assert(!StateOf(leaf).CullsBack() && StateOf(leaf).WritesDepth(), "leaf state");
  static_assert(StateOf(glass).Kind() == SurfaceKind::Refractive, "glass");
  static_assert(StateOf(glass).Blends() && !StateOf(glass).WritesDepth(), "glass state");
  static_assert(StateOf(bark).Kind() == SurfaceKind::Opaque, "bark");

  Check(gAllocations == 0, "Occupy allocated");
  std::printf("sizeof(Body)=%zu trivially_copyable=%d placed=%zu occupied=%u bytes=%zu "
              "occupyAllocations=%ld occupyMs=%.3f\n",
              sizeof(Body), (int)std::is_trivially_copyable<Body>::value, run1.Bodies.size(),
              run1.Occupied, run1.Bodies.size() * sizeof(Body), gAllocations, run1.OccupyMs);
  std::printf("classProbes=%ld classWrong=%ld edgeWrong=%ld runnerUpWrong=%ld finestEdgeM=%.3f "
              "frameOffsetM=%.3f probeNs=%.0f\n",
              classProbes, classWrong, edgeWrong, runnerUpWrong, finestEdgeM, frameOffsetM,
              coverNs);
  std::printf("poolHeapBytes=%zu patchHeapBytes=%zu classBytes=%zu regionSpanEm=%.1f "
              "regionSpanNm=%.1f\n",
              pool.HeapBytes(), snapA.Patch->HeapBytes(), snapA.Classes->Bytes(), a.SpanEm(),
              a.SpanNm());
  if (gFailures) std::printf("verify-generators: %d failed\n", gFailures);
  return gFailures ? 1 : 0;
}
