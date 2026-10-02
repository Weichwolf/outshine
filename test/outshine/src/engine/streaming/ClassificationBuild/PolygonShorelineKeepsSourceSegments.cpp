#include "ClassificationBuild.h"
#include "Check.h"

#include <utility>

int main() {
  using namespace outshine;
  using namespace outshine::Ground;
  using namespace outshine::Test;
  ClassificationBuild::Job job;
  job.Frame = TangentFrame::At({});
  job.Raster.CellM = 2;
  job.Raster.HalfCells = 16;
  job.Raster.Points = {-12, -12, 12, -12, 12, 12, -12, 12, -4, -4, -4, 4, 4, 4, 4, -4};
  job.Raster.Rings = {{0, 4}, {4, 4}};
  job.Raster.Features = {{.FirstRing = 0,
                          .RingCount = 2,
                          .Rank = 1,
                          .ClassRow = 3,
                          .Form = Generators::ClassificationRasterizer::Shape::Polygon,
                          .MinE = -12,
                          .MinN = -12,
                          .MaxE = 12,
                          .MaxN = 12}};
  Tasks compute(1);
  ClassificationBuild builder(compute);
  CHECK(builder.Submit(std::move(job)), "classification uses the shared compute queue");
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
