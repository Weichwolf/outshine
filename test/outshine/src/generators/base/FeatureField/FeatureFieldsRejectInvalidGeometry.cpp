#include <array>
#include <limits>
#include "src/generators/base/FeatureField.h"
#include "Check.h"

int main() {
  using namespace outshine::Generators;
  using namespace outshine::Test;
  using Field = FeatureField;
  const std::array<Field::Vertex, 4> vertices{{{1, 1}, {3, 1}, {3, 3}, {1, 3}}};
  const std::array<Field::Ring, 1> rings{{{0, 4}}};
  Field::Feature area{};
  area.RingCount = 1;
  area.Kind = FeatureKind::Structure;
  area.Form = FeatureForm::Area;
  const auto build = [&](Field::Feature feature) {
    return Field::Of(std::array{feature}, rings, vertices);
  };
  const auto field = build(area);
  CHECK(field != nullptr, "valid square accepted");
  if (!field) { return Report(); }
  CHECK(field->Contains(field->At(0), {.EastM = 2, .NorthM = 2}), "square contains its centre");
  CHECK(!field->Contains(field->At(0), {.EastM = 0, .NorthM = 2}), "square excludes outside point");
  for (float value :
       {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
    auto bad = vertices;
    bad[1].Em = value;
    CHECK(!Field::Of(std::array{area}, rings, bad), "nonfinite vertex rejected");
    auto feature = area;
    feature.HalfWidthM = value;
    CHECK(!build(feature), "nonfinite width rejected");
    feature = area;
    feature.Top = FeatureLevel::At(value);
    CHECK(!build(feature), "nonfinite height rejected");
  }
  auto invalid = area;
  invalid.HalfWidthM = -1;
  CHECK(!build(invalid), "negative width rejected");
  invalid = area;
  invalid.Form = static_cast<FeatureForm>(255);
  CHECK(!build(invalid), "unknown form rejected");
  invalid = area;
  invalid.Kind = static_cast<FeatureKind>(255);
  CHECK(!build(invalid), "unknown kind rejected");
  invalid = area;
  invalid.FirstRing = std::numeric_limits<uint32_t>::max();
  CHECK(!build(invalid), "invalid ring range rejected");
  const std::array<Field::Ring, 1> badRing{{{std::numeric_limits<uint32_t>::max(), 4}}};
  CHECK(!Field::Of(std::array{area}, badRing, vertices), "invalid vertex range rejected");
  auto ribbon = area;
  ribbon.Form = FeatureForm::Ribbon;
  ribbon.HalfWidthM = 0.5f;
  const auto road = build(ribbon);
  CHECK(road && road->Contains(road->At(0), {.EastM = 2, .NorthM = 0.5}),
        "ribbon includes width boundary");
  CHECK(road && !road->Contains(road->At(0), {.EastM = 2, .NorthM = 0.4}),
        "ribbon excludes beyond width");
  const auto maximum = std::numeric_limits<float>::max();
  const std::array<Field::Vertex, 2> large{{{maximum, 0}, {maximum, 1}}};
  const std::array<Field::Ring, 1> pair{{{0, 2}}};
  ribbon.HalfWidthM = maximum;
  CHECK(!Field::Of(std::array{ribbon}, pair, large), "overflowing expanded bounds rejected");
  CHECK(Field::Of({}, {}, {}) != nullptr, "empty snapshot is valid");
  return Report();
}
