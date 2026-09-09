#include <generation/Generate.h>
#include <cmath>
#include <limits>
#include "Check.h"

using outshine::Generators::Unseen;
static_assert(Unseen(0.3, 691.0, 300.0));
static_assert(!Unseen(0.3, 691.0, 100.0));
static_assert(!Unseen(100.0, 691.0, 10000.0));
static_assert(Unseen(100.0, 691.0, 100000.0));
static_assert(Unseen(0.0, 691.0, 1.0));
static_assert(!Unseen(1.0, 0.0, 1.0));

int main() {
  using namespace outshine::Test;
  const double nan = std::numeric_limits<double>::quiet_NaN();
  const double infinity = std::numeric_limits<double>::infinity();
  for (double invalid : {-1.0, nan, infinity, -infinity}) {
    CHECK(!Unseen(invalid, 1.0, 1.0), "invalid geometry error cannot justify simplification");
    CHECK(!Unseen(1.0, invalid, 1.0), "invalid focal length cannot justify simplification");
    CHECK(!Unseen(1.0, 1.0, invalid), "invalid distance cannot justify simplification");
    CHECK(!Unseen(0.0, invalid, 1.0), "zero error does not legitimize an invalid camera");
    CHECK(!Unseen(0.0, 1.0, invalid), "zero error does not legitimize an invalid distance");
  }
  CHECK(!Unseen(0.0, 0.0, 1.0) && !Unseen(0.0, 1.0, 0.0), "zero projection dimensions are invalid");
  CHECK(Unseen(1.0, 1.0, 1.0), "exactly one pixel meets the policy");
  CHECK(!Unseen(std::nextafter(1.0, 2.0), 1.0, 1.0),
        "the next representable larger error exceeds it");
  CHECK(Unseen(std::nextafter(1.0, 0.0), 1.0, 1.0),
        "the next representable smaller error meets it");
  CHECK(Unseen(0.0, 1.0, 1.0) && Unseen(-0.0, 1.0, 1.0),
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
        CHECK(Unseen(error, focal, distance) == old(error, focal, distance),
              "valid dyadic inputs retain the previous projection decision");
      }
    }
  }
  Covers("finite nonnegative displacement and finite positive projection; previous examples; "
         "one-pixel boundary; invalid-input controls; not correctness of consumer error bounds");
  return Report();
}
