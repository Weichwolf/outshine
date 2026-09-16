#include "ClassBuilder.h"
#include "Check.h"
#include <memory>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Ground;
using namespace outshine::Test;

ClassBuilder::Job Polygon(bool overlay) {
  ClassBuilder::Job job;
  job.Frame = TangentFrame::At({});
  job.CellM = 2;
  job.HalfCells = 16;
  // Opposite windings: outer square and inner hole. Probes avoid curved corners.
  job.Pts = {-12, -12, 12, -12, 12, 12, -12, 12, -4, -4, -4, 4, 4, 4, 4, -4};
  job.Rings = {{0, 4}, {4, 4}};
  job.Feats = {{.FirstRing = 0,
                .RingCount = 2,
                .Rank = 1,
                .Tpl = 3,
                .Form = ClassBuilder::Shape::Polygon,
                .MinE = -12,
                .MinN = -12,
                .MaxE = 12,
                .MaxN = 12}};
  if (overlay) {
    auto feature = job.Feats.front();
    feature.RingCount = 1;
    feature.Rank = 2;
    feature.Tpl = 7;
    job.Feats.push_back(feature);
  }
  return job;
}

std::shared_ptr<const ClassStructure> Build(ClassBuilder &builder, ClassBuilder::Job job) {
  builder.Submit(std::move(job));
  CHECK(builder.AwaitCompletion(5.0), "small raster job wakes its waiter within five seconds");
  const auto result = builder.Collect();
  CHECK(result.has_value(), "a completed raster job is collectable after its wake");
  return result ? result->Structure : nullptr;
}
}

int main() {
  ClassBuilder builder;
  const auto hole = Build(builder, Polygon(false));
  CHECK(hole != nullptr, "polygon field published");
  if (!hole) { return Report(); }
  CHECK(hole->Evaluate(8, 0, nullptr, nullptr) == 3, "outer interior has polygon class");
  CHECK(hole->Evaluate(0, 0, nullptr, nullptr) == -1, "opposite winding leaves central hole");
  CHECK(hole->Evaluate(24, 0, nullptr, nullptr) == -1, "exterior remains unclassified");
  const auto overlay = Build(builder, Polygon(true));
  CHECK(overlay != nullptr, "overlapping field published");
  if (!overlay) { return Report(); }
  CHECK(overlay->Evaluate(8, 0, nullptr, nullptr) == 7, "higher rank wins overlapping interior");
  CHECK(overlay->Evaluate(0, 0, nullptr, nullptr) == 7, "new polygon fills former hole");
  auto emptyJob = Polygon(false);
  emptyJob.Feats.clear();
  const auto empty = Build(builder, std::move(emptyJob));
  CHECK(empty && empty->Evaluate(8, 0, nullptr, nullptr) == -1,
        "empty next job contains no stale base or seeds");
  CHECK(hole->Evaluate(8, 0, nullptr, nullptr) == 3 && hole->Evaluate(0, 0, nullptr, nullptr) == -1,
        "old immutable snapshot survives scratch reuse");
  auto lineJob = Polygon(false);
  lineJob.Pts = {-10, 0, 10, 0};
  lineJob.Rings = {{0, 2}};
  lineJob.Feats = {{.FirstRing = 0,
                    .RingCount = 1,
                    .Rank = 1,
                    .Tpl = 4,
                    .Form = ClassBuilder::Shape::Line,
                    .WidthM = 8,
                    .MinE = -14,
                    .MinN = -4,
                    .MaxE = 14,
                    .MaxN = 4}};
  const auto line = Build(builder, std::move(lineJob));
  CHECK(line && line->Evaluate(0, 0, nullptr, nullptr) == 4, "line width covers its center");
  CHECK(line && line->Evaluate(0, 8, nullptr, nullptr) == -1,
        "line width leaves distant cells unclassified");
  CHECK(line && line->Version() == 4 && line->Measured().Overflow == 0,
        "each completed job advances its version without overflowing simple cases");
  return Report();
}
