#include "geo/Geodesy.h"
#include "Check.h"

#include <cmath>
#include <cstdint>
#include <limits>

namespace {
constexpr uint64_t kDegreesPerTurn = 360;

uint64_t PowerOfTwoResidue(int exponent) {
  uint64_t residue = 1;
  for (int bit = 0; bit < exponent; ++bit) { residue = residue * 2u % kDegreesPerTurn; }
  return residue;
}

double SignedResidue(uint64_t residue) {
  const auto held = static_cast<int64_t>(residue);
  return static_cast<double>(held > 180 ? held - static_cast<int64_t>(kDegreesPerTurn) : held);
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  for (int degrees = -1080; degrees <= 1080; ++degrees) {
    int expected = degrees % 360;
    if (expected > 180) { expected -= 360; }
    if (expected < -180) { expected += 360; }
    CHECK(Wrap180(static_cast<double>(degrees)) == static_cast<double>(expected),
          "ordinary integer degrees retain the independent signed integer remainder");
  }
  for (int turns = -10; turns <= 10; ++turns) {
    CHECK(Wrap180(13.5 + static_cast<double>(turns) * kDegPerTurn) == 13.5 &&
              Wrap180(-13.5 + static_cast<double>(turns) * kDegPerTurn) == -13.5,
          "exact fractional positions are independent of longitude turns");
  }
  CHECK(Wrap180(180.0) == 180.0 && Wrap180(-180.0) == -180.0 &&
            Wrap180(540.0) == 180.0 && Wrap180(-540.0) == -180.0,
        "the existing signs at the antimeridian remain part of the math contract");
  CHECK(!std::signbit(Wrap180(0.0)) && std::signbit(Wrap180(-0.0)) &&
            !std::signbit(Wrap180(360.0)) && std::signbit(Wrap180(-360.0)),
        "zero keeps its input sign including exact full turns");
  for (int exponent = 0; exponent < std::numeric_limits<double>::max_exponent; ++exponent) {
    const double magnitude = std::ldexp(1.0, exponent);
    const double expected = SignedResidue(PowerOfTwoResidue(exponent));
    CHECK(Wrap180(magnitude) == expected && Wrap180(-magnitude) == -expected,
          "extreme exactly representable values match independent integer modular arithmetic");
  }
  constexpr int digits = std::numeric_limits<double>::digits;
  static_assert(digits < std::numeric_limits<uint64_t>::digits);
  const uint64_t mantissa = (uint64_t{1} << digits) - 1u;
  const uint64_t residue = mantissa % kDegreesPerTurn *
                           PowerOfTwoResidue(std::numeric_limits<double>::max_exponent - digits) %
                           kDegreesPerTurn;
  const double expected = SignedResidue(residue);
  const double maximum = std::numeric_limits<double>::max();
  CHECK(Wrap180(maximum) == expected && Wrap180(-maximum) == -expected,
        "maximum finite doubles use their exact integer mantissa and exponent oracle");
  CHECK(std::isnan(Wrap180(std::numeric_limits<double>::infinity())) &&
            std::isnan(Wrap180(-std::numeric_limits<double>::infinity())) &&
            std::isnan(Wrap180(std::numeric_limits<double>::quiet_NaN())),
        "nonfinite longitudes return a nonfinite diagnostic without a runaway loop");
  return Report();
}
