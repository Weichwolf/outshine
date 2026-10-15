#include "ClassificationBuild.h"
#include "Check.h"
#include <utility>

int main() {
  using namespace outshine;
  using namespace outshine::Ground;
  using namespace outshine::Test;
  using Rasterizer = Generators::ClassificationRasterizer;
  ClassificationBuild::Job job;
  job.Frame = TangentFrame::At({});
  job.Raster.CellM = 16;
  job.Raster.HalfCells = 2;
  job.Raster.Points = {0, 0, 8, 0, 8, 8, 0, 8, 4, 0, 12, 0, 12, 8, 4, 8};
  job.Raster.Rings = {{.First = 0, .Count = 4, .Role = Rasterizer::RingRole::Exterior},
                      {.First = 4, .Count = 4, .Role = Rasterizer::RingRole::Exterior}};
  job.Raster.Features = {{.FirstRing = 0,
                          .RingCount = 1,
                          .Rank = 10,
                          .ClassRow = 3,
                          .MinE = 0,
                          .MinN = 0,
                          .MaxE = 8,
                          .MaxN = 8},
                         {.FirstRing = 1,
                          .RingCount = 1,
                          .Rank = 10,
                          .ClassRow = 3,
                          .MinE = 4,
                          .MinN = 0,
                          .MaxE = 12,
                          .MaxN = 8}};
  const auto points = job.Raster.Points;
  Tasks compute(1);
  ClassificationBuild builder(compute);
  CHECK(builder.Submit(std::move(job)) && builder.AwaitCompletion(5),
        "region compilation shares the asynchronous worker");
  const auto result = builder.Collect();
  CHECK(result && result->Structure && result->Upload, "the compiled native product publishes");
  if (!result || !result->Structure) { return Report(); }
  CHECK(result->Returned.Raster.Points == points && result->Returned.Raster.Features.size() == 2 &&
            result->Returned.Raster.Rings.size() == 2,
        "source lending returns the original input rather than the merged derivative");
  const auto &field = *result->Structure;
  CHECK(field.Evaluate(1, 1, nullptr, nullptr) == 3 &&
            field.Evaluate(6, 1, nullptr, nullptr) == 3 &&
            field.Evaluate(11, 1, nullptr, nullptr) == 3 &&
            field.Evaluate(13, 1, nullptr, nullptr) == -1,
        "the runtime compiles overlapping source coverage without extending it");
  double distance = 0;
  CHECK(field.Evaluate(6, 4, &distance, nullptr) == 3 && distance == 4 &&
            field.Measured().Overflow == 0,
        "the nearest merged boundary excludes the two internal overlapping source edges");
  CHECK(result->BuildMs > 0 && result->BuildMs == field.Measured().BuildMs,
        "the published build cost includes region compilation and rasterization");
  return Report();
}
