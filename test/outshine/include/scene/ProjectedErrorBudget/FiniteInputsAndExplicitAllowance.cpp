#include <scene/ProjectedErrorBudget.h>
#include <cmath>
#include <limits>
#include "Check.h"

int main() {
  using namespace outshine::Test;
  using outshine::ProjectedErrorBudget;
  const auto allows = [](double error, double focal, double distance) {
    return ProjectedErrorBudget{.FocalPx = focal}.Allows(error, distance);
  };
  CHECK(allows(0.3, 691.0, 300.0), "existing finite projection example");
  CHECK(!allows(0.3, 691.0, 100.0), "existing finite projection example");
  CHECK(!allows(100.0, 691.0, 10000.0), "existing finite projection example");
  CHECK(allows(100.0, 691.0, 100000.0), "existing finite projection example");
  CHECK(allows(0.0, 691.0, 1.0), "existing finite projection example");
  CHECK(!allows(1.0, 0.0, 1.0), "existing finite projection example");

  const double nan = std::numeric_limits<double>::quiet_NaN();
  const double infinity = std::numeric_limits<double>::infinity();
  for (double invalid : {-1.0, nan, infinity, -infinity}) {
    CHECK(!allows(invalid, 1.0, 1.0), "invalid geometry error cannot justify simplification");
    CHECK(!allows(1.0, invalid, 1.0), "invalid focal length cannot justify simplification");
    CHECK(!allows(1.0, 1.0, invalid), "invalid distance cannot justify simplification");
    CHECK(!allows(0.0, invalid, 1.0), "zero error does not legitimize an invalid camera");
    CHECK(!allows(0.0, 1.0, invalid), "zero error does not legitimize an invalid distance");
  }
  CHECK(!allows(0.0, 0.0, 1.0) && !allows(0.0, 1.0, 0.0), "zero projection dimensions are invalid");
  CHECK(allows(1.0, 1.0, 1.0), "exactly one pixel meets the policy");
  CHECK(!allows(std::nextafter(1.0, 2.0), 1.0, 1.0),
        "the next representable larger error exceeds it");
  CHECK(allows(std::nextafter(1.0, 0.0), 1.0, 1.0),
        "the next representable smaller error meets it");
  CHECK(allows(0.0, 1.0, 1.0) && allows(-0.0, 1.0, 1.0),
        "both signed zeros represent no displacement");
  const auto old = [](double error, double focal, double distance) {
    if (!(error > 0.0)) { return true; }
    if (!(focal > 0.0) || !(distance > 0.0)) { return false; }
    return error * focal <= distance;
  };
  CHECK(old(nan, 1, 1) && old(-1, 1, 1), "the former predicate exhibits both measured defects");
  for (double error : {0.0, 0.125, 1.0, 8.0}) {
    for (double focal : {1.0, 64.0, 1024.0}) {
      for (double distance : {1.0, 128.0, 8192.0}) {
        CHECK(allows(error, focal, distance) == old(error, focal, distance),
              "valid dyadic inputs retain the previous projection decision");
      }
    }
  }
  const ProjectedErrorBudget strict{.FocalPx = 800.0, .AllowedErrorPx = 0.25};
  const ProjectedErrorBudget broad{.FocalPx = 800.0, .AllowedErrorPx = 2.0};
  CHECK(!strict.Allows(0.125, 100.0) && broad.Allows(0.125, 100.0),
        "a 100-pixel-metre displacement fails a 25 budget and fits a 200 budget");
  CHECK(!(strict == broad), "different allowances identify different detail policies");
  const ProjectedErrorBudget exact{.FocalPx = 800.0, .AllowedErrorPx = 0.0};
  CHECK(exact.Allows(0.0, 100.0) && !exact.Allows(0.125, 100.0),
        "an exact geometry policy permits only zero displacement");
  for (double invalid : {-1.0, nan, infinity, -infinity}) {
    CHECK((!ProjectedErrorBudget{.FocalPx = 1.0, .AllowedErrorPx = invalid}.Allows(0.0, 1.0)),
          "invalid allowances remain invalid with exact geometry");
  }
  const double largest = std::numeric_limits<double>::max();
  const ProjectedErrorBudget overflowing{.FocalPx = largest, .AllowedErrorPx = largest};
  CHECK(!overflowing.Allows(largest, largest),
        "two overflowing sides cannot turn infinity equality into a simplification proof");
  const double smallest = std::numeric_limits<double>::denorm_min();
  CHECK(!ProjectedErrorBudget{.FocalPx = smallest}.Allows(smallest, 1.0),
        "a positive displacement is not exact geometry when its estimate underflows");
  CHECK(!ProjectedErrorBudget{}.Allows(0.0, 1.0),
        "an unspecified projection cannot authorize simplification");
  Covers("finite nonnegative displacement and finite positive projection; previous examples; "
         "one-pixel boundary; invalid-input controls; not correctness of consumer error bounds");
  return Report();
}
