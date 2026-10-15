#include "ClassRegionCompiler.h"
#include "ClassStructure.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <initializer_list>
#include <memory>
#include <span>
#include <stop_token>
#include <vector>

namespace {
using Rasterizer = outshine::Generators::ClassificationRasterizer;
using Point = std::array<float, 2>;

struct Ring {
  Rasterizer::RingRole Role;
  std::vector<Point> Points;
};

void Polygon(Rasterizer::Input &input, uint16_t row, int rank, std::initializer_list<Ring> rings) {
  Rasterizer::Feature feature{.FirstRing = static_cast<uint32_t>(input.Rings.size()),
                              .RingCount = static_cast<uint32_t>(rings.size()),
                              .Rank = rank,
                              .ClassRow = row,
                              .Form = Rasterizer::Shape::Polygon,
                              .MinE = 100,
                              .MinN = 100,
                              .MaxE = -100,
                              .MaxN = -100};
  for (const auto &ring : rings) {
    input.Rings.push_back({.First = static_cast<uint32_t>(input.Points.size() / 2),
                           .Count = static_cast<uint32_t>(ring.Points.size()),
                           .Role = ring.Role});
    for (const auto &point : ring.Points) {
      input.Points.insert(input.Points.end(), point.begin(), point.end());
      feature.MinE = std::min(feature.MinE, point[0]);
      feature.MinN = std::min(feature.MinN, point[1]);
      feature.MaxE = std::max(feature.MaxE, point[0]);
      feature.MaxN = std::max(feature.MaxN, point[1]);
    }
  }
  input.Features.push_back(feature);
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  using enum Rasterizer::RingRole;
  Rasterizer::Input source;
  source.CellM = 1;
  source.HalfCells = 32;
  Polygon(source,
          3,
          10,
          {{Exterior, {{0, 0}, {8, 0}, {8, 8}, {0, 8}}}, {Hole, {{2, 2}, {2, 6}, {6, 6}, {6, 2}}}});
  Polygon(source, 3, 10, {{Exterior, {{4, 0}, {10, 0}, {10, 8}, {4, 8}}}});
  Polygon(source, 3, 10, {{Exterior, {{20, 20}, {22, 20}, {22, 22}, {20, 22}}}});
  const auto saved = source.Points;
  const auto compiled = CompileClassRegions(source);
  CHECK((compiled && compiled->CoalescedGroups == 1 && compiled->Raster.Features.size() == 1 &&
         compiled->Contributors == std::vector<std::vector<uint32_t>>{{0, 1, 2}}),
        "equal material/rank regions retain every original source index");
  if (!compiled) { return Report(); }
  CHECK(source.Points == saved, "compilation does not mutate retained source coordinates");
  Rasterizer rasterizer;
  const auto before = rasterizer.Build(source);
  const auto after = rasterizer.Build(compiled->Raster);
  CHECK(before && after && before->Overflow == 0 && after->Overflow == 0,
        "both representations fit the existing raster budget");
  if (!before || !after) { return Report(); }
  const auto empty = std::make_shared<const ClassStructure::Grid>();
  const ClassStructure original(TangentFrame::At({}), before->Grid, empty, {});
  const ClassStructure merged(TangentFrame::At({}), after->Grid, empty, {});
  for (int north = -2; north < 25; ++north) {
    for (int east = -2; east < 25; ++east) {
      CHECK(original.Evaluate(east + .137, north + .293, nullptr, nullptr) ==
                merged.Evaluate(east + .137, north + .293, nullptr, nullptr),
            "complete source coverage, holes and disconnected islands survive union");
    }
  }
  CHECK(merged.Evaluate(3, 3, nullptr, nullptr) == -1 &&
            merged.Evaluate(5, 3, nullptr, nullptr) == 3 &&
            merged.Evaluate(21, 21, nullptr, nullptr) == 3,
        "the remaining hole, filled overlap and distant island stay explicit");
  Polygon(source, 4, 10, {{Exterior, {{1, 1}, {3, 1}, {3, 3}, {1, 3}}}});
  const auto ambiguous = CompileClassRegions(source);
  CHECK((ambiguous && ambiguous->CoalescedGroups == 0 && ambiguous->Raster.Features.size() == 4 &&
         ambiguous->Contributors == std::vector<std::vector<uint32_t>>{{0}, {1}, {2}, {3}}),
        "a rank naming different materials retains original order and individual sources");
  std::stop_source cancellation;
  cancellation.request_stop();
  CHECK(!CompileClassRegions(source, cancellation.get_token()), "cancelled work cannot publish");
  return Report();
}
