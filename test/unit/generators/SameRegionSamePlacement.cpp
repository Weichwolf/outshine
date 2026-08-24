#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "Check.h"

#include "ClassStructure.h"
#include "Buildings.h"
#include "Infrastructure.h"
#include "FeatureLevel.h"
#include "Forest.h"
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
#include "Water.h"
#include "WaterDepth.h"
#include "Yield.h"

using namespace outshine;
using namespace outshine::Generators;

namespace {

long gAllocations = 0;
bool gCounting = false;

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

  const float ring[4][4] = {{(float)west, -1000.0f, (float)east, -1000.0f},
                            {(float)east, -1000.0f, (float)east, 5000.0f},
                            {(float)east, 5000.0f, (float)west, 5000.0f},
                            {(float)west, 5000.0f, (float)west, -1000.0f}};
  for (const auto &e : ring)
    for (float v : e) g.Edges.push_back(v);
}

std::shared_ptr<const ClassStructure> SyntheticClasses(const TangentFrame &frame) {
  auto fine = std::make_shared<ClassStructure::Grid>();
  fine->W = fine->H = 1;
  fine->OrgE = fine->OrgN = -100.0;
  fine->CellM = 4000.0;
  fine->Cells = {0xFFu | (2u << 16), 0u};
  const uint32_t wind = 128u;
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

constexpr int32_t kBuiltRow = 1, kWetRow = 2, kWayRow = 0;
constexpr float kRoofAslM = 560.0f, kLevelAslM = 430.0f, kBaseAslM = 470.0f;

constexpr float kHalfWayM = 3.0f;

std::shared_ptr<const FeatureField> SyntheticFeatures(const Region &region) {
  const float spanE = (float)region.SpanEm(), spanN = (float)region.SpanNm();
  const FeatureField::Feature features[3] = {
      {0, 1, kBuiltRow, FeatureKind::Structure, FeatureForm::Area, 0.0f,
       FeatureLevel::At(kBaseAslM), FeatureLevel::At(kRoofAslM), 0, 0, 0, 0},
      {1, 1, kWetRow, FeatureKind::Water, FeatureForm::Area, 0.0f, FeatureLevel::None(),
       FeatureLevel::At(kLevelAslM), 0, 0, 0, 0},
      {2, 1, kWayRow, FeatureKind::Way, FeatureForm::Ribbon, kHalfWayM, FeatureLevel::None(),
       FeatureLevel::None(), 0, 0, 0, 0}};
  const FeatureField::Ring rings[3] = {{0, 4}, {4, 4}, {8, 2}};
  const FeatureField::Vertex vertices[10] = {
      {0.10f * spanE, 0.10f * spanN}, {0.20f * spanE, 0.10f * spanN},
      {0.20f * spanE, 0.20f * spanN}, {0.10f * spanE, 0.20f * spanN},
      {0.40f * spanE, 0.40f * spanN}, {0.90f * spanE, 0.40f * spanN},
      {0.90f * spanE, 0.60f * spanN}, {0.40f * spanE, 0.60f * spanN},
      {0.10f * spanE, 0.80f * spanN}, {0.90f * spanE, 0.80f * spanN}};
  return FeatureField::Of(Span<const FeatureField::Feature>(features, 3),
                          Span<const FeatureField::Ring>(rings, 3),
                          Span<const FeatureField::Vertex>(vertices, 10));
}

Ground::Snapshot SyntheticSnapshot(const Region &region) {
  std::vector<GroundPatch::Posting> postings((size_t)kSide * kSide);
  for (int j = 0; j < kSide; j++) {
    for (int i = 0; i < kSide; i++) {
      GroundPatch::Posting &p = postings[(size_t)j * kSide + (size_t)i];
      const double e = (double)i / (kSide - 1), n = (double)j / (kSide - 1);
      p.Height = GroundSample::At(400.0 + 120.0 * e + 40.0 * std::sin(12.0 * n));
    }
  }
  std::vector<GroundTable::Row> rows(3);
  rows[0].SlopeMaxDeg = 35.0f;
  rows[1].SlopeMaxDeg = 60.0f;
  rows[2].SlopeMaxDeg = 5.0f;
  rows[1].Surface.BaseColour[3] = 0.4f;
  rows[1].Surface.Alpha = outshine::AlphaMode::Masked;
  Ground::Snapshot s;
  s.Patch = GroundPatch::Complete(region, kSide, Span<const GroundPatch::Posting>(
                                                     postings.data(), postings.size()));
  s.Classes = SyntheticClasses(TangentFrame::At(region.AnchorLat(), region.AnchorLon()));
  s.Features = SyntheticFeatures(region);
  s.Table = GroundTable::Of(Span<const GroundTable::Row>(rows.data(), rows.size()));
  return s;
}

struct Seam {
  double MinStepM = 1.0e30, MaxStepM = 0.0, CrossMinM = 1.0e30, CrossMaxM = 0.0;
  long Rows = 0, Stands = 0;
};

std::vector<Body> Grown(RegionPool &pool, const Ground &ground, const Generator &gen,
                        Span<Yield::Note> notes) {
  std::optional<RegionPool::Lease> lease = pool.TryAcquire(ground);
  Yield yield(lease->Sink(), gen.NoteNames(), notes);
  gen.Occupy(ground, yield);
  const Span<const Body> placed = lease->Sink().Placed();
  return std::vector<Body>(placed.begin(), placed.end());
}

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

  uint32_t Proposes(double areaM2) const noexcept override {
    return (uint32_t)(areaM2 / (StepM_ * StepM_) + 1.0);
  }

  [[nodiscard]] bool At(const Ground &ground, double eastM, double northM, Body *out) const noexcept override {
    int row = 0;
    const Cover cover = ground.CoverAt(eastM, northM);
    if (!cover.TryRow(&row)) return false;
    if (ground.SlopeDeg(eastM, northM) > (double)ground.Table().At((size_t)row).SlopeMaxDeg)
      return false;
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
  std::vector<uint32_t> Conflicts;
  uint32_t Occupied = 0, Outside = 0, Full = 0;
  double OccupyMs = 0.0;
};

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

[[nodiscard]] bool Same(const Yield::Note &a, const Yield::Note &b) {
  return a.Name == b.Name && a.Times == b.Times && a.Raised == b.Raised &&
         (!a.Raised || a.Peak == b.Peak);
}

}

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
  CHECK(a.Seed() != b.Seed(), "two regions share a seed");
  CHECK(Region::Of(14, 51.96, 9.55).Is(Region::Of(14, 51.96, 9.55)), "Region::Of is not a function");

  double lat = 0.0, lon = 0.0, e = 0.0, n = 0.0;
  a.Geo(1234.5, 678.9, &lat, &lon);
  a.Enu(lat, lon, &e, &n);
  CHECK(std::fabs(e - 1234.5) < 1e-6 && std::fabs(n - 678.9) < 1e-6, "Enu/Geo do not round-trip");

  const Ground::Snapshot snapA = SyntheticSnapshot(a), snapB = SyntheticSnapshot(b);
  const std::optional<Ground> groundA = Ground::Of(a, snapA), groundB = Ground::Of(b, snapB);
  CHECK(groundA.has_value() && groundB.has_value(), "a complete snapshot yielded no Ground");
  Ground::Snapshot classless = snapA;
  classless.Classes.reset();
  CHECK(!Ground::Of(a, classless), "a Ground was offered with no classifier");

  double frameOffsetM = 0.0;
  for (int j = 0; j <= 8; j++)
    for (int i = 0; i <= 8; i++) {
      const double em = a.SpanEm() * (double)i / 8.0, nm = a.SpanNm() * (double)j / 8.0;
      a.Geo(em, nm, &lat, &lon);
      snapA.Classes->Frame().Project(lat, lon, &e, &n);
      frameOffsetM = std::max(frameOffsetM, std::max(std::fabs(e - em), std::fabs(n - nm)));
    }

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
  CHECK(classWrong == 0, "the class disagreed with the outlines it came from");
  CHECK(edgeWrong == 0, "the distance to the wood line disagreed with the wood line");
  CHECK(runnerUpWrong == 0, "the runner-up disagreed with the outline under the winner");

  std::vector<GroundPatch::Posting> holed((size_t)kSide * kSide);
  holed[17].Height = GroundSample::Missing();
  for (size_t i = 0; i < holed.size(); i++)
    if (i != 17) holed[i].Height = GroundSample::At(300.0);
  CHECK(!GroundPatch::Complete(a, kSide, Span<const GroundPatch::Posting>(holed.data(),
                                                                         holed.size())),
        "a patch with a hole in it was offered");

  const FeatureField::Vertex verts[4] = {{10.0f, 20.0f}, {40.0f, 20.0f}, {40.0f, 60.0f},
                                         {10.0f, 60.0f}};
  const FeatureField::Ring rings[1] = {{0, 4}};
  FeatureField::Feature feats[1] = {{0, 1, 7, FeatureKind::Structure, FeatureForm::Area, 0.0f,
                                    FeatureLevel::None(), FeatureLevel::None(), 0.0f, 0.0f, 0.0f,
                                    0.0f}};
  const std::shared_ptr<const FeatureField> field =
      FeatureField::Of(Span<const FeatureField::Feature>(feats, 1),
                       Span<const FeatureField::Ring>(rings, 1),
                       Span<const FeatureField::Vertex>(verts, 4));
  CHECK(field && field->At(0).MinEm == 10.0f && field->At(0).MaxEm == 40.0f &&
            field->At(0).MinNm == 20.0f && field->At(0).MaxNm == 60.0f,
        "the feature box was not derived from the rings");
  CHECK(field && !FeatureField::Boxed(field->At(0), 41.0, 30.0) &&
            FeatureField::Boxed(field->At(0), 20.0, 30.0),
        "the feature box holds the wrong points");
  CHECK(field && field->Contains(field->At(0), 20.0, 30.0) &&
            !field->Contains(field->At(0), 200.0, 30.0),
        "the outline holds the wrong points");

  const Scatter scatter(30.0), sparse(97.0);
  GeneratorSet set;
  CHECK(set.Add(Rank{10}, scatter), "the first registration was refused");
  CHECK(!set.Add(Rank{10}, scatter), "a duplicate rank was accepted");
  CHECK(set.Add(Rank{20}, sparse), "the second registration was refused");
  const size_t noteCount = 2 * Scatter::kNotes;
  std::vector<Yield::Note> notes(noteCount), notesB(noteCount), notes2(noteCount),
      notes3(noteCount);

  const Schedule schedule(Schedule::Ring{14, 2});

  const std::optional<Region> widest = schedule.Widest(51.96, 9.55);
  CHECK(widest.has_value(), "the ring named no region to size the pool on");
  for (size_t i = 0; i < schedule.Count(); i++) {
    const std::optional<Region> member = schedule.At(i, 51.96, 9.55);
    CHECK(!member || member->SpanEm() * member->SpanNm() <= widest->SpanEm() * widest->SpanNm(),
          "a ring member is wider than the one the schedule calls widest");
  }
  RegionPool pool(RegionPool::Extent{a, schedule.Broadest()},
                  RegionPool::Shape{2, kBodies, kCellM});

  CHECK(sparse.Proposes(a.SpanEm() * a.SpanNm()) > 0,
        "a registered generator proposes nothing over a whole region");
  CHECK(sparse.Proposes(4.0 * a.SpanEm() * a.SpanNm()) > sparse.Proposes(a.SpanEm() * a.SpanNm()),
        "a generator's proposal does not grow with the ground it is asked about");
  {
    std::optional<RegionPool::Lease> first = pool.TryAcquire(*groundA);
    std::optional<RegionPool::Lease> second = pool.TryAcquire(*groundA);
    CHECK(first && second && !pool.TryAcquire(*groundA),
          "the pool handed out more sinks than it holds");
    second.reset();
    CHECK(pool.Free() == 1, "a released lease did not come back");
  }
  CHECK(pool.Free() == 2, "the pool did not come back whole");

  const Run run1 = Filled(pool, *groundA, set, notes);
  const Run runB = Filled(pool, *groundB, set, notesB);
  const Run run2 = Filled(pool, *groundA, set, notes2);

  std::optional<RegionPool::Lease> held = pool.TryAcquire(*groundA);
  const Run run3 = Filled(pool, *groundA, set, notes3);
  held.reset();

  CHECK(!run1.Bodies.empty(), "nothing was placed at all");
  CHECK(run1.Occupied > 0, "nothing was ever refused, so the claim was never tested");
  CHECK(run1.Outside == 0 && run1.Full == 0, "the scatter left its own region or ran out of room");
  CHECK(run1.Bodies.size() == run2.Bodies.size() && run1.Bodies.size() == run3.Bodies.size(),
        "the count moved");
  CHECK(run1.Bodies.size() == run2.Bodies.size() &&
            std::memcmp(run1.Bodies.data(), run2.Bodies.data(),
                        run1.Bodies.size() * sizeof(Body)) == 0,
        "the same region in the same sink placed different bytes");
  CHECK(run1.Bodies.size() == run3.Bodies.size() &&
            std::memcmp(run1.Bodies.data(), run3.Bodies.data(),
                        run1.Bodies.size() * sizeof(Body)) == 0,
        "the same region in another sink placed different bytes");
  CHECK(run1.Occupied == run2.Occupied && run1.Occupied == run3.Occupied, "the refusals moved");
  CHECK(run1.Bodies.size() != runB.Bodies.size() ||
            std::memcmp(run1.Bodies.data(), runB.Bodies.data(),
                        run1.Bodies.size() * sizeof(Body)) != 0,
        "two different regions placed the same thing");

  bool sameNotes = true;
  for (size_t i = 0; i < notes.size(); i++) sameNotes = sameNotes && Same(notes[i], notes2[i]);
  CHECK(sameNotes, "the same region kept different notes");
  CHECK(run1.Ranges.size() == 2 && run1.Ranges[0].First == 0 &&
            run1.Ranges[1].First == run1.Ranges[0].Count &&
            run1.Ranges[0].Count + run1.Ranges[1].Count == run1.Bodies.size(),
        "the two generators' stretches of the sink do not tile what was placed");
  CHECK(run1.Ranges.size() == 2 && run1.Ranges[1].Count > 0,
        "the second generator placed nothing, so its stretch proves nothing");
  CHECK(notes[Scatter::HighestStandAslM].Raised && notes[Scatter::HighestStandAslM].Peak > 400.0,
        "the generator's high-water did not land");
  CHECK(run1.Conflicts.size() == 2 && notes[Scatter::Occupied].Times == run1.Conflicts[0] &&
            run1.Conflicts[0] + run1.Conflicts[1] == run1.Occupied,
        "the yields do not partition the sink's refusals");
  CHECK(std::strcmp(notes[Scatter::InWater].Name, "inWater") == 0,
        "a note lost the name its generator declared");

  {

    const float perM2[3] = {0.2f, 0.2f, 0.2f};
    const Forest forest(Forest::Stem{}, Span<const float>(perM2, 3), AlpineLimit());
    const double em = a.SpanEm() / (double)(int)(a.SpanEm() / Forest::kCellM + 0.5);
    const double nm = a.SpanNm() / (double)(int)(a.SpanNm() / Forest::kCellM + 0.5);
    RegionPool wide(RegionPool::Extent{a, schedule.Broadest()},
                    RegionPool::Shape{2, 262144, kCellM});
    std::vector<Yield::Note> seamNotes(Forest::kNotes), seamNotesB(Forest::kNotes);
    const std::vector<Body> west =
        Grown(wide, *groundA, forest, Span<Yield::Note>(seamNotes.data(), seamNotes.size()));
    const std::vector<Body> east =
        Grown(wide, *groundB, forest, Span<Yield::Note>(seamNotesB.data(), seamNotesB.size()));
    // board:1541: a world carries the species that grow in it, and that is 0 or 1..N.
    // src/assets/world/species/ holds 31 of them while the forest took exactly one, so a
    // world was a stand of a single tree and every check still passed.
    {
      Forest::Stem mixed[3];
      mixed[0].HeightM = 10.0;
      mixed[0].TrunkRadiusM = 0.10f;
      mixed[1].HeightM = 20.0;
      mixed[1].TrunkRadiusM = 0.20f;
      mixed[2].HeightM = 30.0;
      mixed[2].TrunkRadiusM = 0.30f;
      const Forest wood(Span<const Forest::Stem>(mixed, 3), Span<const float>(perM2, 3),
                        AlpineLimit());
      CHECK(wood.SpeciesCount() == 3, "a forest declared over three species holds three");

      std::vector<Yield::Note> mixedNotes(Forest::kNotes);
      const std::vector<Body> stand =
          Grown(wide, *groundA, wood, Span<Yield::Note>(mixedNotes.data(), mixedNotes.size()));
      std::map<int, int> byHeight;
      for (const Body &one : stand) { ++byHeight[(int)(one.HeightM / 5.0f)]; }
      std::printf("NOTE trees the mixed stand grew = %zu trees\n", stand.size());
      std::printf("NOTE distinct height bands in it = %zu bands\n", byHeight.size());
      CHECK(stand.size() > 100, "the mixed stand grew a wood, not a handful");
      CHECK(byHeight.size() >= 3,
            "**A WOOD IS 0 OR 1..N SPECIES**: three declared stems put three kinds of tree "
            "on the ground, so a world is no longer a stand of one (board:1541)");

      const std::vector<Body> again =
          Grown(wide, *groundA, wood, Span<Yield::Note>(mixedNotes.data(), mixedNotes.size()));
      bool same = again.size() == stand.size();
      for (size_t at = 0; at < again.size() && same; ++at) {
        same = again[at].HeightM == stand[at].HeightM;
      }
      CHECK(same,
            "and which species stands where is a PURE function of the region's own seed -- "
            "the same ground grows the same wood twice");

      // a draw that is pure but BIASED is a wood of one species wearing three names. The
      // stream is a mix of the region seed, so each stem must take its own third of the
      // stand: 1/3 of 198922 with the spread a fair draw of that size actually shows.
      std::map<int, int> byStem;
      for (const Body &one : stand) { ++byStem[(int)(one.HeightM / 5.0f)]; }
      double leastShare = 1.0, mostShare = 0.0;
      for (const auto &[band, count] : byStem) {
        (void)band;
        const double share = (double)count / (double)stand.size();
        leastShare = share < leastShare ? share : leastShare;
        mostShare = share > mostShare ? share : mostShare;
      }
      std::printf("NOTE the smallest species share = %.4f of it\n", leastShare);
      std::printf("NOTE the largest species share = %.4f of it\n", mostShare);
      // three standard deviations of a fair binomial draw at n = 198922, p = 1/3 is
      // 3 * sqrt(p(1-p)/n) = 0.0032, so a fair draw sits inside 1/3 +- 0.0032 essentially
      // always; the bar is set at ten times that, which no fair draw fails and no stuck
      // stream passes.
      const double fair = 1.0 / (double)wood.SpeciesCount();
      CHECK(leastShare > fair - 0.032 && mostShare < fair + 0.032,
            "**AND NO SPECIES TAKES MORE OF THE WOOD THAN ITS SHARE**: the draw is a mix of "
            "the region's seed, so three stems each take a third -- a pure draw that is "
            "biased is a wood of one species wearing three names (board:1541)");
    }

    // board:1781: the forest capped at kMostSpecies without a word, and answered a wood of
    // ZERO species with the same note as ground that carries no cover row -- so a
    // misconfigured world published exactly the telemetry of bare rock.
    {
      std::vector<Forest::Stem> tooMany(Forest::kMostSpecies + 1);
      for (size_t at = 0; at < tooMany.size(); ++at) {
        tooMany[at].HeightM = 5.0 + (double)at;
      }
      const Forest capped(Span<const Forest::Stem>(tooMany.data(), tooMany.size()),
                          Span<const float>(perM2, 3), AlpineLimit());
      std::printf("NOTE species declared = %zu, held = %zu, refused = %zu\n", tooMany.size(),
                  capped.SpeciesCount(), capped.SpeciesRefused());
      // board:1781: kMostSpecies had no origin, and this hour made it PUBLIC so a test could
      // spell it -- widening the surface of an underived constant. Its origin is the cache:
      // the table is indexed once per cell of every region, so it stands beside Ground and
      // Yield rather than behind a pointer.
      std::printf("NOTE the species table = %zu bytes of the %zu it is allowed\n",
                  sizeof(Forest::Stem) * Forest::kMostSpecies, Forest::kSpeciesTableBytes);
      CHECK(sizeof(Forest::Stem) * Forest::kMostSpecies <= Forest::kSpeciesTableBytes,
            "**THE POOL'S SIZE HAS AN ORIGIN**: the species table is a cache resident, not a "
            "heap indirection, and the bound is what fits beside the cell loop's own working "
            "set (board:1781)");
      CHECK(capped.SpeciesCount() == Forest::kMostSpecies && capped.SpeciesRefused() == 1,
            "**A WOOD THAT DECLARES MORE SPECIES THAN THE POOL HOLDS SAYS SO**: the count it "
            "kept and the count it turned away are both published, because a pool that "
            "silently drops a declaration is a pool the author cannot argue with (board:1781)");

      const Forest bare(Span<const Forest::Stem>(nullptr, 0), Span<const float>(perM2, 3),
                        AlpineLimit());
      std::vector<Yield::Note> bareNotes(Forest::kNotes);
      const std::vector<Body> nothing =
          Grown(wide, *groundA, bare, Span<Yield::Note>(bareNotes.data(), bareNotes.size()));
      std::printf("NOTE trees a wood of no species grows = %zu\n", nothing.size());
      std::printf("NOTE its noSpecies note = %lu, its noTemplate note = %lu\n",
                  (unsigned long)bareNotes[Forest::NoSpecies].Times,
                  (unsigned long)bareNotes[Forest::NoTemplate].Times);
      CHECK(nothing.empty(), "a wood of no species grows no tree");
      CHECK(bareNotes[Forest::NoSpecies].Times > 0,
            "**AND AN EMPTY WOOD SAYS WHICH EMPTINESS IT IS**: 'noSpecies' is not 'noTemplate' "
            "-- a misconfigured world may not publish the telemetry of bare rock (board:1781)");
    }

    CHECK(a.SpanEm() == b.SpanEm(), "two regions of one Mercator row disagree on their width");

    constexpr double kBandM = 100.0;
    std::map<long, std::vector<double>> byRow;
    for (const Body &body : west)
      if (body.Em > a.SpanEm() - kBandM) byRow[(long)(body.Nm / nm)].push_back(body.Em);
    for (const Body &body : east)
      if (body.Em < kBandM) byRow[(long)(body.Nm / nm)].push_back(a.SpanEm() + body.Em);
    Seam seam;
    for (auto &row : byRow) {
      std::sort(row.second.begin(), row.second.end());
      seam.Rows++;
      seam.Stands += (long)row.second.size();
      for (size_t i = 1; i < row.second.size(); i++) {
        const double step = row.second[i] - row.second[i - 1];
        const bool crosses = row.second[i - 1] < a.SpanEm() && row.second[i] >= a.SpanEm();
        seam.MinStepM = std::min(seam.MinStepM, step);
        seam.MaxStepM = std::max(seam.MaxStepM, step);
        if (crosses) {
          seam.CrossMinM = std::min(seam.CrossMinM, step);
          seam.CrossMaxM = std::max(seam.CrossMaxM, step);
        }
      }
    }
    CHECK(seam.Stands > 10000 && !west.empty() && !east.empty(),
          "the two regions placed nothing to compare");

    CHECK(seam.MinStepM >= 0.5 * em - 1.0e-6 && seam.MaxStepM <= 1.5 * em + 1.0e-6,
          "the lattice has a gap or a doubled stand somewhere");
    CHECK(seam.CrossMinM >= 0.5 * em - 1.0e-6 && seam.CrossMaxM <= 1.5 * em + 1.0e-6,
          "the two regions do not meet: the border pair is closer or wider than one lattice step");
    std::printf("seamRows=%ld seamStands=%ld cellEm=%.4f stepMinM=%.4f stepMaxM=%.4f "
                "crossMinM=%.4f crossMaxM=%.4f\n",
                seam.Rows, seam.Stands, em, seam.MinStepM, seam.MaxStepM, seam.CrossMinM,
                seam.CrossMaxM);
  }

  const std::optional<BodyId> id = run1.Bodies[0].Id();
  CHECK(id && id->Index() == 0, "a placed body carries no id");
  CHECK(!Body().Id(), "an unplaced body carries an id");

  {
    std::optional<RegionPool::Lease> lease = pool.TryAcquire(*groundA);
    Body outside = run1.Bodies[0];
    outside.Em = a.SpanEm() + 1.0;
    CHECK(lease->Sink().Place(outside).Why() == Claim::Outcome::Outside,
          "a body outside the region was not refused as outside");
    BodyId placed = *run1.Bodies[0].Id();
    CHECK(lease->Sink().Place(run1.Bodies[0]).TryId(&placed) &&
              placed.Index() == 0,
          "the first body of an open sink was not claimed");
    CHECK(lease->Sink().Place(run1.Bodies[0]).Why() == Claim::Outcome::Occupied,
          "a body inside one already standing was not refused as occupied");
  }

  RegionPool tiny(RegionPool::Extent{a, schedule.Broadest()}, RegionPool::Shape{1, 4, kCellM});
  std::vector<Yield::Note> tinyNotes(noteCount);
  const Run cramped = Filled(tiny, *groundA, set, tinyNotes);
  CHECK(cramped.Bodies.size() == 4 && cramped.Full == 2,
        "a full sink did not say so once to each generator");

  CHECK(schedule.Count() == 25, "the ring is not (2R+1)^2");
  const std::optional<Region> nearest = schedule.At(0, 51.96, 9.55);
  CHECK(nearest && nearest->Is(Region::Of(14, 51.96, 9.55)), "the nearest region is not the centre");

  constexpr Material leaf{{0.1f, 0.3f, 0.1f}, 0.6f, 0.35f, 0.4f, 1.0f, {0.0f, 0.0f, 0.0f}};
  constexpr Material glass{{0.0f, 0.0f, 0.0f}, 0.05f, 1.0f, 0.9f, 1.5f, {0.0f, 0.0f, 0.0f}};
  constexpr Material bark{};
  static_assert(StateOf(leaf).Kind() == SurfaceKind::ThinTransmissive, "leaf");
  static_assert(!StateOf(leaf).CullsBack() && StateOf(leaf).WritesDepth(), "leaf state");

  static_assert(StateOf(glass).Kind() == SurfaceKind::ThinTransmissive, "a pane with no volume");
  static_assert(!StateOf(glass).CullsBack(), "a thin wall is seen from both sides");
  constexpr Material solid = [](Material made) {
    made.Thickness = 0.01f;
    return made;
  }(glass);
  static_assert(StateOf(solid).Kind() == SurfaceKind::Refractive, "the same pane given a volume");
  static_assert(StateOf(solid).Blends() && !StateOf(solid).WritesDepth(), "a volume's state");
  static_assert(StateOf(bark).Kind() == SurfaceKind::Opaque, "bark");

  {
    const Buildings built(ContactMaterial{2});
    const Water wet;
    Body roof;
    CHECK(built.At(*groundA, 0.15 * a.SpanEm(), 0.15 * a.SpanNm(), &roof),
          "nothing stands under the roofed outline");
    CHECK(std::fabs(roof.BaseAslM + (double)roof.HeightM - (double)kRoofAslM) < 0.01,
          "the roof the query answers is not the roof the outline carries");

    CHECK(std::fabs(roof.BaseAslM - (double)kBaseAslM) < 0.01,
          "the queried prism does not stand on the outline's own base");
    CHECK(!built.At(*groundA, 0.50 * a.SpanEm(), 0.15 * a.SpanNm(), &roof),
          "a roof stands outside its own outline");
    CHECK(built.Proposes(a.SpanEm() * a.SpanNm()) == 0 && wet.Proposes(1.0e9) == 0,
          "an outline generator proposed occupancy it does not claim");

    long dry = 0, standing = 0, disagreeing = 0;
    double deepest = 0.0, worst = 0.0;
    for (int j = 0; j <= 64; j++)
      for (int i = 0; i <= 64; i++) {
        const double em = a.SpanEm() * (double)i / 64.0, nm = a.SpanNm() * (double)j / 64.0;
        const WaterDepth d = wet.DepthAt(*groundA, em, nm);
        CHECK(wet.DepthAt(*groundA, em, nm).Where() == d.Where(), "the depth is not a function");
        double m = 0.0;
        switch (d.Where()) {
          case WaterDepth::State::Dry: dry++; break;
          case WaterDepth::State::Standing:
            standing++;
            CHECK(d.TryDepthM(&m) && m >= 0.0, "a standing depth was not a non-negative metre");
            deepest = m > deepest ? m : deepest;
            break;
          case WaterDepth::State::LevelBelowGround:
            disagreeing++;
            CHECK(d.TryDisagreementM(&m) && m >= 0.0, "a disagreement was not a non-negative metre");
            CHECK(!d.TryDepthM(&m), "a disagreement answered with a depth");
            worst = m > worst ? m : worst;
            break;
        }
      }
    CHECK(standing > 0 && disagreeing > 0,
          "the water probe reached neither a standing depth nor a disagreement");
    std::printf("waterProbes=%ld dry=%ld standing=%ld levelBelowGround=%ld deepestM=%.3f "
                "worstDisagreementM=%.3f\n",
                (long)(65 * 65), dry, standing, disagreeing, deepest, worst);

    const Infrastructure ways;
    const double on = 0.80 * a.SpanNm(), mid = 0.50 * a.SpanEm();
    CHECK(ways.MadeAt(*groundA, mid, on).has_value(), "nothing is made on the way's own centreline");
    CHECK(ways.MadeAt(*groundA, mid, on)->WidthM == 2.0f * kHalfWayM,
          "the way answers a width that is not the one it was declared with");
    CHECK(std::fabs(ways.MadeAt(*groundA, mid, on)->SurfaceAslM -
                    groundA->HeightAslM(mid, on)) < 1.0e-9,
          "a way's surface is not the ground it is graded onto");
    CHECK(ways.MadeAt(*groundA, mid, on + (double)kHalfWayM - 0.01).has_value(),
          "the way stops short of its own half-width");
    CHECK(!ways.MadeAt(*groundA, mid, on + (double)kHalfWayM + 0.01).has_value(),
          "the way reaches past its own half-width");
    CHECK(!ways.MadeAt(*groundA, mid, 0.5 * a.SpanNm()).has_value(),
          "a way was made where none runs");
    CHECK(ways.Proposes(a.SpanEm() * a.SpanNm()) == 0,
          "an outline generator proposed occupancy it does not claim");
  }

  CHECK(gAllocations == 0, "Occupy allocated");
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
  return outshine::Test::Report();
}
