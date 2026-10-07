#include "Check.h"
#include "src/generators/base/FeatureField.h"

#include <array>
#include <limits>

int main() {
  using namespace outshine::Generators;
  using namespace outshine::Test;
  using Field = FeatureField;
  const std::array<Field::Vertex, 8> vertices{
      {{0, 0}, {10, 0}, {10, 10}, {0, 10}, {2, 2}, {8, 2}, {8, 8}, {2, 8}}};
  const std::array<Field::Ring, 2> rings{{{0, 4}, {4, 4}}};
  const std::array<Field::Feature, 1> features{{{.FirstRing = 0,
                                                 .RingCount = 2,
                                                 .CoverRow = 0,
                                                 .Kind = FeatureKind::Structure,
                                                 .Form = FeatureForm::Area}}};
  const auto field = Field::Of(features, rings, vertices);
  CHECK(field != nullptr, "a courtyard supplies valid occupied rings");
  if (!field) { return Report(); }
  const auto &area = field->At(0);
  CHECK(field->Intersects(area, {.EastM = 1, .NorthM = 1}, 0),
        "an occupied centre intersects even with zero radius");
  CHECK(!field->Intersects(area, {.EastM = 5, .NorthM = 5}, 2.9) &&
            field->Intersects(area, {.EastM = 5, .NorthM = 5}, 3),
        "a courtyard remains free until a circle reaches its wall");
  CHECK(!field->Intersects(area, {.EastM = 10.5, .NorthM = 5}, .49) &&
            field->Intersects(area, {.EastM = 10.5, .NorthM = 5}, .5),
        "outside centres account for radius and exact boundary contact");
  CHECK(!field->Intersects(area, {.EastM = 10.4, .NorthM = 10.4}, .5) &&
            field->Intersects(area, {.EastM = 10.4, .NorthM = 10.4}, .6),
        "bounding box overlap alone does not occupy a circular corner");
  for (const double radius :
       {-1., std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()}) {
    CHECK(!field->Intersects(area, {.EastM = 1, .NorthM = 1}, radius),
          "invalid radii cannot become valid intersections");
  }
  auto ribbon = features;
  ribbon[0].Form = FeatureForm::Ribbon;
  ribbon[0].RingCount = 1;
  ribbon[0].HalfWidthM = 1;
  const std::array<Field::Ring, 1> line{{{0, 2}}};
  const auto road = Field::Of(ribbon, line, vertices);
  CHECK(road != nullptr, "a ribbon supplies a valid segment");
  if (road) {
    CHECK(road->Intersects(road->At(0), {.EastM = 5, .NorthM = 1.5}, .5) &&
              !road->Intersects(road->At(0), {.EastM = 5, .NorthM = 1.6}, .5),
          "ribbon clearance adds the query radius to its half width");
    CHECK(road->Intersects(road->At(0), {.EastM = -1.5, .NorthM = 0}, .5),
          "the query radius also extends circular ribbon end caps");
  }
  return Report();
}
