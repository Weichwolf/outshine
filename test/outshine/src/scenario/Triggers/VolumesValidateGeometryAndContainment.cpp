#include "Triggers.h"
#include "Check.h"
#include <array>
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const std::array events{Scenario::Event{.Name = "entered"}};
  Scenario::Volume volume{
      .Shape = "sphere", .ExtentM = {1, 1, 1}, .Fires = "entered", .When = "enter"};
  const auto prepare = [&] { return TriggerField::Stand(std::span(&volume, 1), events); };
  const double infinity = std::numeric_limits<double>::infinity();
  const double nan = std::numeric_limits<double>::quiet_NaN();
  for (int axis = 0; axis < 3; ++axis) {
    for (double bad : {infinity, -infinity, nan}) {
      volume.AtM[axis] = bad;
      CHECK(!prepare(), "nonfinite centers are rejected");
      volume.AtM[axis] = 0;
    }
    for (double bad : {-1.0, infinity, nan}) {
      volume.ExtentM[axis] = bad;
      CHECK(!prepare(), "invalid extents are rejected");
      volume.ExtentM[axis] = 1;
    }
  }
  volume.When = "dwell";
  for (double bad : {0.0, -1.0, infinity, nan}) {
    volume.DwellS = bad;
    CHECK(!prepare(), "dwell requires a finite positive duration");
  }
  volume.When = "enter";
  volume.DwellS = 0;
  const auto contains = [&](const Vec3 &point) {
    auto field = prepare();
    CHECK(field.has_value(), "finite geometry prepares");
    if (!field) { return false; }
    CHECK(field->Probe({.Index = 1, .Generation = 1}, point, 0).has_value(),
          "valid trigger probe accepted");
    return !field->Drain().empty();
  };
  CHECK(contains({1, 0, 0}), "sphere boundary is included");
  CHECK(!contains({1, 1, 0}), "sphere excludes the bounding-box corner");
  volume.ExtentM = {1e200, 0, 0};
  CHECK(contains({1e200, 0, 0}), "large sphere boundary remains included");
  CHECK(!contains({2e200, 0, 0}), "squared overflow cannot produce false containment");
  volume.ExtentM = {1e-200, 0, 0};
  CHECK(!contains({2e-200, 0, 0}), "squared underflow cannot produce false containment");
  volume.Shape = "box";
  volume.ExtentM = {1, 2, 3};
  CHECK(contains({1, -2, 3}), "box corners are included");
  CHECK(!contains({0, 0, 4}), "box excludes points beyond any extent");
  volume.ExtentM = {};
  CHECK(contains({}), "zero extents retain their center");
  CHECK(!contains({1e-200, 0, 0}), "zero extents contain only their center");
  return Report();
}
