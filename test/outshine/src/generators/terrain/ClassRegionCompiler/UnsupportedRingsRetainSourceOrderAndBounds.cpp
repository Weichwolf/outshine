#include "ClassRegionCompiler.h"
#include "Check.h"
#include <limits>
#include <vector>

int main() {
  using namespace outshine::Generators;
  using namespace outshine::Test;
  using Rasterizer = ClassificationRasterizer;
  Rasterizer::Input source;
  source.CamE = 123;
  source.CamN = -456;
  source.CellM = 16;
  source.HalfCells = 32;
  source.Points = {0, 0, 8, 0, 8, 8, 0, 8, 2, 2, 2, 6, 6, 6, 6, 2};
  source.Rings = {{0, 4}, {4, 4}};
  source.Features = {{.FirstRing = 0,
                      .RingCount = 2,
                      .Rank = 10,
                      .ClassRow = 3,
                      .MinE = 0,
                      .MinN = 0,
                      .MaxE = 8,
                      .MaxN = 8},
                     {.FirstRing = 0,
                      .RingCount = 1,
                      .Rank = 10,
                      .ClassRow = 3,
                      .MinE = 0,
                      .MinN = 0,
                      .MaxE = 8,
                      .MaxN = 8}};
  const auto retained = CompileClassRegions(source);
  CHECK((retained && retained->CoalescedGroups == 0 && retained->RetainedGroups == 1 &&
         retained->Raster.Features.size() == 2 &&
         retained->Contributors == std::vector<std::vector<uint32_t>>{{0}, {1}}),
        "unspecified multi-ring topology retains every source instead of guessing holes");
  if (retained) {
    const auto &raster = retained->Raster;
    CHECK(raster.CamE == 123 && raster.CamN == -456 && raster.CellM == 16 &&
              raster.HalfCells == 32 && raster.Features[0].RingCount == 2 &&
              raster.Features[1].RingCount == 1 && raster.Features[0].MinE == 0 &&
              raster.Features[0].MaxN == 8,
          "fallback retains the source frame, grid demand, ordering and declared bounds");
  }
  source.Features[1].Form = Rasterizer::Shape::Line;
  source.Features[1].WidthM = 5;
  const auto line = CompileClassRegions(source);
  CHECK(line && line->Raster.Features.size() == 2 &&
            line->Raster.Features[1].Form == Rasterizer::Shape::Line &&
            line->Raster.Features[1].WidthM == 5 && line->CoalescedGroups == 0,
        "line width and shape cannot join a polygon union");
  source.Rings[0].First = std::numeric_limits<uint32_t>::max();
  CHECK(!CompileClassRegions(source), "invalid source ranges cannot publish partial geometry");
  source.Rings[0].First = 0;
  source.Points.push_back(1);
  CHECK(!CompileClassRegions(source), "incomplete source coordinate pairs are rejected");
  const auto empty = CompileClassRegions({});
  CHECK(empty && empty->Raster.Features.empty() && empty->Contributors.empty(),
        "an empty generation publishes an empty product");
  return Report();
}
