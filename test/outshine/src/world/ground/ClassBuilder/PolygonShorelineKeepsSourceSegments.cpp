#include "ClassBuilder.h"
#include "Check.h"

#include <utility>

int main() {
  using namespace outshine;
  using namespace outshine::Ground;
  using namespace outshine::Test;
  ClassBuilder::Job job;
  job.Frame = TangentFrame::At({});
  job.CellM = 2;
  job.HalfCells = 16;
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
  ClassBuilder builder;
  builder.Submit(std::move(job));
  CHECK(builder.AwaitCompletion(5), "shoreline classification completes");
  const auto result = builder.Collect();
  CHECK(result.has_value(), "shoreline classification publishes");
  if (!result) { return Report(); }
  const auto &field = *result->Structure;
  CHECK(field.Evaluate(0, -11.5, nullptr, nullptr) == 3,
        "water remains inside the source outer boundary");
  CHECK(field.Evaluate(0, -12.5, nullptr, nullptr) == -1,
        "water does not extend beyond the straight source shoreline");
  CHECK(field.Evaluate(0, -4.5, nullptr, nullptr) == 3,
        "water reaches the straight source island boundary");
  CHECK(field.Evaluate(0, -3.5, nullptr, nullptr) == -1, "island interior remains dry");
  CHECK(field.Evaluate(11.5, 11.5, nullptr, nullptr) == 3,
        "source polygon corners remain represented");
  for (double north = -15.5; north < 16; north += 1) {
    for (double east = -15.5; east < 16; east += 1) {
      const bool outer = east > -12 && east < 12 && north > -12 && north < 12;
      const bool island = east > -4 && east < 4 && north > -4 && north < 4;
      CHECK(field.Evaluate(east, north, nullptr, nullptr) == (outer && !island ? 3 : -1),
            "subcell classification agrees with independent rectangular set difference");
    }
  }
  return Report();
}
