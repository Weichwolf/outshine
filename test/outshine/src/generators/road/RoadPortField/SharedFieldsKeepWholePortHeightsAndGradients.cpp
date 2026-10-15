#include "Check.h"
#include "RoadPortField.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <span>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;

std::array<RoadPortField::Plane, 2> Planes() {
  return {{{.Origin = {}, .HeightM = 170, .Gradient = {.SlopeE = .02, .SlopeN = .012}},
           {.Origin = {.EastM = 100, .NorthM = 0},
            .HeightM = 173,
            .Gradient = {.SlopeE = -.01, .SlopeN = .006}}}};
}

RoadPortField::Patch Patch() {
  return {.Parents = {0, 1},
          .CutCentres = {{{}, {.EastM = 100, .NorthM = 0}}},
          .InwardNormals = {{{.EastM = 1, .NorthM = 0}, {.EastM = -1, .NorthM = 0}}},
          .ScaleM = 100};
}

RoadPortField::Sample Polynomial(double x, double y) {
  const double xx = x * x;
  const double xxx = xx * x;
  return {.HeightM = 170 + .02 * x + .012 * y + .0012 * xx - .000017 * xxx + .00000006 * xxx * x -
                     .0000018 * xx * y + .000000012 * xxx * y,
          .Gradient = {.SlopeE = .02 + .0024 * x - .000051 * xx + .00000024 * xxx -
                                 .0000036 * x * y + .000000036 * xx * y,
                       .SlopeN = .012 - .0000018 * xx + .000000012 * xxx}};
}

EastNorth Rotated(EastNorth at) {
  constexpr double angle = .71;
  return {.EastM = std::cos(angle) * at.EastM - std::sin(angle) * at.NorthM,
          .NorthM = std::sin(angle) * at.EastM + std::cos(angle) * at.NorthM};
}

EastNorth Placed(EastNorth at) {
  const auto rotated = Rotated(at);
  return {.EastM = rotated.EastM + 25000, .NorthM = rotated.NorthM - 12000};
}

void CheckPolynomialAndFrame() {
  auto planes = Planes();
  auto patch = Patch();
  for (auto &plane : planes) {
    plane.Origin = Placed(plane.Origin);
    const auto gradient =
        Rotated({.EastM = plane.Gradient.SlopeE, .NorthM = plane.Gradient.SlopeN});
    plane.Gradient = {.SlopeE = gradient.EastM, .SlopeN = gradient.NorthM};
  }
  for (size_t end = 0; end < 2; ++end) {
    patch.CutCentres[end] = Placed(patch.CutCentres[end]);
    patch.InwardNormals[end] = Rotated(patch.InwardNormals[end]);
  }
  for (const double scale : {1.e-8, 1.0, 100.0, 4.e7}) {
    patch.ScaleM = scale;
    const auto field = RoadPortField::Make(planes, std::span{&patch, 1u});
    CHECK(field.has_value(), "metric scale and rigid frame retain the same mathematical field");
    if (!field) { continue; }
    for (int x = -10; x <= 110; x += 2) {
      for (const double y : {-4., -1., 0., 2., 4.}) {
        const auto value =
            field->Evaluate(0, Placed({.EastM = static_cast<double>(x), .NorthM = y}));
        CHECK(value.has_value(), "analytic continuation is not clamped to a finite road domain");
        if (!value) { continue; }
        const auto expected = Polynomial(x, y);
        const auto gradient =
            Rotated({.EastM = expected.Gradient.SlopeE, .NorthM = expected.Gradient.SlopeN});
        CHECK(std::abs(value->HeightM - expected.HeightM) < 1.e-10 &&
                  std::abs(value->Gradient.SlopeE - gradient.EastM) < 1.e-11 &&
                  std::abs(value->Gradient.SlopeN - gradient.NorthM) < 1.e-11,
              "independently expanded polynomial and XY derivatives agree after rotation");
      }
    }
  }
}

void CheckWholeObliquePorts() {
  auto planes = Planes();
  std::array patches{Patch(), Patch(), Patch()};
  patches[0].InwardNormals[1] = {.EastM = -.8, .NorthM = .6};
  patches[1].Parents = {0, 0};
  patches[2].Parents = {1, 1};
  const auto field = RoadPortField::Make(planes, patches);
  CHECK(field.has_value(), "actual nonparallel cut lines define a rational transition");
  if (!field) { return; }
  planes[0].HeightM += 100;
  patches[0].Parents = {99, 99};
  for (const double y : {-4., -2., 0., 1., 4.}) {
    for (size_t end = 0; end < 2; ++end) {
      const EastNorth at{.EastM = end == 0 ? 0.0 : 100 + .75 * y, .NorthM = y};
      const auto blended = field->Evaluate(0, at);
      const auto parent = field->Evaluate(1 + end, at);
      CHECK(blended && parent, "owned inputs retain both whole shared ports");
      if (!blended || !parent) { continue; }
      CHECK(std::abs(blended->HeightM - parent->HeightM) < 1.e-11 &&
                std::abs(blended->Gradient.SlopeE - parent->Gradient.SlopeE) < 1.e-11 &&
                std::abs(blended->Gradient.SlopeN - parent->Gradient.SlopeN) < 1.e-11,
            "entire oblique ports share height and the complete two-dimensional gradient");
    }
  }
  constexpr double step = .001;
  for (const double x : {10., 37., 81.}) {
    const auto at = field->Evaluate(0, {.EastM = x, .NorthM = 2});
    const auto east = field->Evaluate(0, {.EastM = x + step, .NorthM = 2});
    const auto west = field->Evaluate(0, {.EastM = x - step, .NorthM = 2});
    const auto north = field->Evaluate(0, {.EastM = x, .NorthM = 2 + step});
    const auto south = field->Evaluate(0, {.EastM = x, .NorthM = 2 - step});
    CHECK(at && east && west && north && south, "interior finite differences remain in the field");
    if (!at || !east || !west || !north || !south) { continue; }
    CHECK(std::abs((east->HeightM - west->HeightM) / (2 * step) - at->Gradient.SlopeE) < 1.e-8 &&
              std::abs((north->HeightM - south->HeightM) / (2 * step) - at->Gradient.SlopeN) <
                  1.e-8,
          "nonconstant rational denominator includes both gradient contributions");
  }
}

void CheckBoundaries() {
  const auto planes = Planes();
  auto patch = Patch();
  const auto field = RoadPortField::Make(planes, std::span{&patch, 1u});
  CHECK(field.has_value(), "valid bounded field constructs");
  if (!field) { return; }
  CHECK(!field->Evaluate(1, {}), "query patch indices are checked before access");
  for (const double invalid : {std::numeric_limits<double>::quiet_NaN(),
                               std::numeric_limits<double>::infinity(),
                               4.e7 + 1}) {
    CHECK(!field->Evaluate(0, {.EastM = invalid, .NorthM = 0}), "query coordinates stay bounded");
  }
  patch.Parents[1] = 2;
  CHECK(!RoadPortField::Make(planes, std::span{&patch, 1u}), "missing parents are rejected");
  patch = Patch();
  patch.InwardNormals[0].EastM = 2;
  CHECK(!RoadPortField::Make(planes, std::span{&patch, 1u}), "cut normals must actually be unit");
  patch = Patch();
  patch.CutCentres[1] = {};
  const auto singular = RoadPortField::Make(planes, std::span{&patch, 1u});
  CHECK(singular && !singular->Evaluate(0, {}),
        "nonpositive rational denominators fail explicitly");
  auto invalid = planes;
  invalid[0].HeightM = std::numeric_limits<double>::infinity();
  CHECK(!RoadPortField::Make(invalid, std::span{&patch, 1u}), "plane coefficients must be finite");
  CHECK(!RoadPortField::Make({}, std::span{&patch, 1u}), "empty plane collections are rejected");
  CHECK(!RoadPortField::Make(planes, {}), "empty patch collections are rejected");
  CHECK(!RoadPortField::Make(std::vector<RoadPortField::Plane>(16385), std::span{&patch, 1u}),
        "plane work is bounded before allocating owned state");
  CHECK(!RoadPortField::Make(planes, std::vector<RoadPortField::Patch>(8193)),
        "patch work is bounded before allocating owned state");
}
}

int main() {
  CheckPolynomialAndFrame();
  CheckWholeObliquePorts();
  CheckBoundaries();
  return outshine::Test::Report();
}
