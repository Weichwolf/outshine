#include "StructureSurfaceRefinement.h"
#include "Check.h"
#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Generators;

void Triangle(Raised &mesh, std::array<Vec3f, 3> vertices) {
  for (const auto &vertex : vertices) {
    mesh.RoofRun.push_back(static_cast<uint32_t>(mesh.RoofCorners.size()));
    mesh.RoofCorners.push_back({.pos = vertex});
  }
}

void Rectangle(Raised &mesh, float left, float right, float bottom, float top, float z = 0) {
  Triangle(mesh, {{{{left, bottom, z}}, {{right, bottom, z}}, {{right, top, z}}}});
  Triangle(mesh, {{{{left, bottom, z}}, {{right, top, z}}, {{left, top, z}}}});
}

struct Observation {
  bool Finished = false;
  bool Safe = true;
  bool Paced = true;
  size_t Certificates = 0;
};

Observation
Run(StructureSurfaceRefinementTask &task, uint64_t key, double truth, size_t slice = 1) {
  Observation result;
  for (size_t step = 0; step < 300000; ++step) {
    const size_t before = task.WorkUnits();
    const size_t queries = task.TriangleQueries();
    const auto progress = task.Step(key, slice);
    result.Paced &= task.WorkUnits() - before <= slice && task.TriangleQueries() - queries <= slice;
    if (!progress) {
      result.Safe = false;
      break;
    }
    if (const auto bound = task.Bound(key)) {
      ++result.Certificates;
      result.Safe &= bound->LowerDistanceM() <= truth && truth <= bound->UpperDistanceM();
    }
    if (*progress == StructureSurfaceErrorProgress::Complete) {
      result.Finished = true;
      break;
    }
  }
  return result;
}
}

int main() {
  using namespace outshine::Test;
  Raised cap;
  Raised frame;
  Rectangle(cap, 0, 3, 0, 3);
  Rectangle(frame, 0, 1, 0, 3);
  Rectangle(frame, 2, 3, 0, 3);
  Rectangle(frame, 1, 2, 0, 1);
  Rectangle(frame, 1, 2, 2, 3);
  StructureSurfaceRefinementTask task;
  CHECK(task.Reset({.Reference = cap, .Variant = frame, .SourceKey = 7}), "native opening starts");
  CHECK(task.Step(7, 0) && task.WorkUnits() == 0 && task.TriangleQueries() == 0 && !task.Bound(7),
        "zero slice cannot scan inputs or expose unvalidated coverage");
  const auto opening = Run(task, 7, 0.5);
  const auto openingBound = task.Bound(7);
  CHECK(
      opening.Finished && opening.Safe && opening.Paced && opening.Certificates > 100,
      "every one-unit intermediate certificate encloses the independently known opening distance");
  CHECK(openingBound && openingBound->LowerDistanceM() > 0.48 &&
            openingBound->UpperDistanceM() - openingBound->LowerDistanceM() <= 0.02 &&
            openingBound->Upper.ReferenceToVariantM >= 0.5,
        "complete adaptive proof resolves the filled opening despite zero error at cap vertices");
  CHECK(task.TriangleQueries() <= 131072 && task.RegionCount() <= 4096 && task.Splits() > 0 &&
            task.ScratchCapacityBytes() > 0,
        "precision consumes measured bounded primitive queries, live regions and worker scratch");
  const size_t queries = task.TriangleQueries();
  CHECK(task.Reset({.Reference = cap, .Variant = frame, .SourceKey = 8}), "sliced replay starts");
  const auto replay = Run(task, 8, 0.5, 97);
  CHECK(replay.Finished && replay.Safe && replay.Paced && task.Bound(8) && openingBound &&
            task.Bound(8)->UpperDistanceM() == openingBound->UpperDistanceM() &&
            task.Bound(8)->LowerDistanceM() == openingBound->LowerDistanceM() &&
            task.TriangleQueries() == queries,
        "slice boundaries cannot change deterministic adaptive results");
  CHECK(task.Reset({.Reference = frame, .Variant = cap, .SourceKey = 9}), "reversed pair starts");
  const auto reversed = Run(task, 9, 0.5, 64);
  CHECK(reversed.Finished && reversed.Safe && task.Bound(9) &&
            task.Bound(9)->UpperDistanceM() - task.Bound(9)->LowerDistanceM() <= 0.02 &&
            task.Bound(9)->Upper.VariantToReferenceM >= 0.5,
        "opposite direction retains the opening rather than reporting only the coincident frame");

  for (const size_t capQueries : {size_t{0},
                                  size_t{1},
                                  size_t{31},
                                  size_t{32},
                                  size_t{33},
                                  size_t{34},
                                  size_t{35},
                                  size_t{63},
                                  size_t{67}}) {
    CHECK(task.Reset({.Reference = cap, .Variant = frame, .SourceKey = 10},
                     {.MaxTriangleQueries = capQueries}),
          "exhausted query case starts");
    const auto limited = Run(task, 10, 0.5, 13);
    CHECK(limited.Finished && limited.Safe && limited.Paced && task.Bound(10) &&
              task.TriangleQueries() <= capQueries,
          "initialization or partial child exhaustion retains complete certified parent coverage");
  }
  for (const size_t regions : {size_t{1}, size_t{9}, size_t{10}, size_t{12}, size_t{13}}) {
    CHECK(
        task.Reset({.Reference = cap, .Variant = frame, .SourceKey = 11}, {.MaxRegions = regions}),
        "exhausted region case starts");
    const auto limited = Run(task, 11, 0.5, 19);
    CHECK(limited.Finished && limited.Safe && task.Bound(11) && task.RegionCount() <= regions,
          "insufficient initialization or split capacity never drops triangle coverage");
  }

  Raised plane;
  Raised elevated;
  Rectangle(plane, 0, 2, 0, 2);
  Rectangle(elevated, 0, 2, 0, 2, 3);
  CHECK(task.Reset({.Reference = plane, .Variant = elevated, .SourceKey = 12}),
        "analytic parallel planes start");
  const auto parallel = Run(task, 12, 3, 128);
  CHECK(parallel.Finished && parallel.Safe && task.Bound(12),
        "both directed parallel-plane distances enclose the analytical three metres");
  Raised tessellated;
  Rectangle(tessellated, 0, 1, 0, 2);
  Rectangle(tessellated, 1, 2, 0, 2);
  CHECK(task.Reset({.Reference = plane, .Variant = tessellated, .SourceKey = 13}),
        "identical differently tessellated surface starts");
  const auto identical = Run(task, 13, 0, 128);
  CHECK(identical.Finished && identical.Safe && task.Bound(13) &&
            task.Bound(13)->LowerDistanceM() == 0,
        "all-target minima preserve zero distance despite far unrelated target triangles");
  Raised point;
  Raised otherPoint;
  Triangle(point, {{{{0, 0, 0}}, {{0, 0, 0}}, {{0, 0, 0}}}});
  Triangle(otherPoint, {{{{0, 0, 4}}, {{0, 0, 4}}, {{0, 0, 4}}}});
  CHECK(task.Reset({.Reference = point, .Variant = otherPoint, .SourceKey = 14}),
        "collapsed surfaces start");
  const auto collapsed = Run(task, 14, 4);
  CHECK(collapsed.Finished && collapsed.Safe && task.Bound(14) &&
            task.Bound(14)->UpperDistanceM() - task.Bound(14)->LowerDistanceM() <= 0.02,
        "collapsed triangles remain valid surfaces rather than empty or undefined geometry");

  CHECK(task.Reset({.Reference = cap, .Variant = frame, .SourceKey = 15}), "source guard starts");
  CHECK(task.Step(15, 100) && task.Bound(15), "coarse coverage exists during adaptive work");
  auto copy = task;
  auto moved = std::move(copy);
  const auto originalProgress = Run(task, 15, 0.5, 29);
  const auto movedProgress = Run(moved, 15, 0.5, 29);
  CHECK(originalProgress.Finished && movedProgress.Finished && movedProgress.Safe &&
            task.Bound(15) && moved.Bound(15) &&
            task.Bound(15)->UpperDistanceM() == moved.Bound(15)->UpperDistanceM(),
        "copied and moved tasks retain independent pending child and heap state");
  CHECK(!task.Bound(16), "a result cannot be read with a different source key");
  CHECK(!task.Step(16, 1) && !task.Bound(15), "late source replacement revokes completed proof");
  moved.Cancel();
  CHECK(!moved.Step(15, 1) && !moved.Bound(15), "cancellation revokes every adaptive certificate");
  Raised invalid = frame;
  invalid.RoofRun.back() = std::numeric_limits<uint32_t>::max();
  CHECK(task.Reset({.Reference = cap, .Variant = invalid, .SourceKey = 17}),
        "late bad index remains paced validation");
  CHECK(!task.Step(17, 128) && !task.Bound(17) && task.TriangleQueries() == 0,
        "all native streams validate before any adaptive target query or certificate");
  CHECK(!task.Reset({.Reference = cap, .Variant = frame, .SourceKey = 18}, {.MaxRegions = 0}) &&
            !task.Bound(18),
        "invalid scratch limits fail closed");
  CHECK(!task.Reset({.Reference = cap, .Variant = frame, .SourceKey = 18},
                    {.MaxTriangleQueries = 131073}),
        "query caps cannot exceed the hard work guard");
  CHECK(!task.Reset({.Reference = cap, .Variant = frame, .SourceKey = 18},
                    {.TargetUncertaintyM = std::numeric_limits<double>::quiet_NaN()}),
        "nonfinite uncertainty does not disable stopping guards");
  Raised empty;
  CHECK(task.Reset({.Reference = empty, .Variant = empty, .SourceKey = 19}) && task.Bound(19) &&
            task.Bound(19)->UpperDistanceM() == 0,
        "two empty surfaces preserve exact zero error");
  return Report();
}
