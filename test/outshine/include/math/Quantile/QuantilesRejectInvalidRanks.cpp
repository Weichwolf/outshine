#include <math/Quantile.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include "Check.h"
#include "Shell.h"

using outshine::QuantileError;
using outshine::QuantileOf;

namespace {
constexpr std::array<double, 5> kFive = {1.0, 2.0, 3.0, 4.0, 5.0};
constexpr double kOneStep = 1.0 / static_cast<double>(kFive.size());
static_assert(QuantileOf({}, outshine::kMiddleQuantile).error() == QuantileError::EmptySample);
static_assert(QuantileOf(kFive, 0.0) == kFive.front() && QuantileOf(kFive, 1.0) == kFive.back());
static_assert(QuantileOf(kFive, outshine::kMiddleQuantile) == kFive[2]);
static_assert(QuantileOf(kFive, kOneStep) == kFive.front() &&
              QuantileOf(kFive, kOneStep + kOneStep / 2) == kFive[1]);
static_assert(outshine::kNarrowQuantile > outshine::kNearestQuantile &&
              outshine::kWidestQuantile > outshine::kBroadQuantile);
static_assert(QuantileOf(kFive, outshine::kWidestQuantile) == kFive.back());
static_assert(noexcept(QuantileOf(kFive, kOneStep)));

std::string SanitizedCommand(bool broken) {
  return std::string(
             R"(quantile_tmp=$(mktemp -d "${TMPDIR:-/tmp}/outshine-quantile.XXXXXX") || exit 20
trap 'rm -rf "$quantile_tmp"' EXIT
)") + OUTSHINE_COMPILE +
         " -fno-exceptions -fsanitize=float-cast-overflow -fno-sanitize-recover=all " +
         (broken ? "-DQUANTILE_LEGACY " : "") + R"(-x c++ - -o "$quantile_tmp/check" <<'CPP'
#include <math/Quantile.h>
#include <array>
#include <limits>
int main() {
  const std::array<double, 2> sample = {1, 2};
  volatile double invalid = std::numeric_limits<double>::quiet_NaN();
#ifdef QUANTILE_LEGACY
  const double share = invalid;
  if (share <= 0 || share >= 1) { return 1; }
  const double at = static_cast<double>(sample.size()) * share;
  auto rank = static_cast<size_t>(at);
  if (static_cast<double>(rank) < at) { ++rank; }
  return sample[rank > 0 ? rank - 1 : 0] == 1 ? 0 : 1;
#else
  const auto result = outshine::QuantileOf(sample, invalid);
  return !result && result.error() == outshine::QuantileError::NonFiniteShare ? 0 : 1;
#endif
}
CPP
compiled=$?
[ "$compiled" -eq 0 ] || exit 21
"$quantile_tmp/check" 2>&1
)";
}
} // namespace

int main() {
  using namespace outshine::Test;
  const double infinity = std::numeric_limits<double>::infinity();
  for (const double share : {std::numeric_limits<double>::quiet_NaN(), infinity, -infinity}) {
    const auto result = QuantileOf(kFive, share);
    CHECK(!result && result.error() == QuantileError::NonFiniteShare,
          "non-finite rank has an explicit error before integer conversion");
    const auto empty = QuantileOf({}, share);
    CHECK(!empty && empty.error() == QuantileError::NonFiniteShare,
          "invalid rank takes precedence over an empty sample");
  }
  const auto empty = QuantileOf({}, 0.5);
  CHECK(!empty && empty.error() == QuantileError::EmptySample, "empty is not a zero measurement");
  CHECK(QuantileOf(kFive, -1.0) == 1.0 && QuantileOf(kFive, 2.0) == 5.0,
        "finite ranks outside the interval retain endpoint clamping");
  constexpr std::array<double, 4> sample = {10, 20, 30, 40};
  CHECK(QuantileOf(sample, 0.5) == 20, "nearest rank does not interpolate an even median");
  CHECK(QuantileOf(sample, std::nextafter(0.5, 1.0)) == 30,
        "the next representable rank above a dyadic boundary selects the next observation");
  CHECK(QuantileOf(sample, std::nextafter(0.5, 0.0)) == 20,
        "the preceding rank remains in the lower step");
  constexpr std::array<double, 1> singleton = {-7};
  CHECK(QuantileOf(singleton, 0.01) == -7 && QuantileOf(singleton, 0.99) == -7,
        "a singleton has the same observed value at every valid rank");
  constexpr std::array<double, 4> repeated = {-3, -3, -3, 8};
  CHECK(QuantileOf(repeated, 0.75) == -3 && QuantileOf(repeated, 1) == 8,
        "duplicates and negative measurements remain observations");
  const std::array<double, 3> extended = {-infinity, 0, infinity};
  CHECK(QuantileOf(extended, 0) == -infinity && QuantileOf(extended, 1) == infinity,
        "ordered infinite observations are distinct from invalid rank fractions");

  std::string output;
  CHECK(Run(SanitizedCommand(false), output) == 0,
        "the public header builds without exceptions and rejects NaN under UBSan");
  std::printf("%s", output.c_str());
  output.clear();
  const int legacy = Run(SanitizedCommand(true), output);
  CHECK(legacy != 0 && output.find("runtime error:") != std::string::npos &&
            output.find("nan") != std::string::npos,
        "UBSan detects the former floating-to-integer conversion in a separate process");
  std::printf("%s", output.c_str());

  const std::string prefix =
      std::string(OUTSHINE_COMPILE) +
      " -fno-exceptions -Werror=unused-result -x c++ -fsyntax-only - 2>&1 <<'CPP'\n";
  output.clear();
  CHECK(Run(prefix + "#include <math/Quantile.h>\nvoid check() { auto result = "
                     "outshine::QuantileOf({}, 0.5); (void)result.has_value(); }\nCPP\n",
            output) == 0,
        "consuming the error result compiles with the same warning policy");
  std::printf("%s", output.c_str());
  output.clear();
  const int ignored = Run(
      prefix + "#include <math/Quantile.h>\nvoid check() { outshine::QuantileOf({}, 0.5); }\nCPP\n",
      output);
  CHECK(ignored != 0 && output.find("nodiscard") != std::string::npos,
        "discarding the nodiscard quantile result is a compilation error");
  std::printf("%s", output.c_str());
  Covers("nearest-rank values, finite clamping, explicit empty/non-finite errors, constexpr and "
         "noexcept contracts; exception-free public compilation, nodiscard compiler control and "
         "UBSan legacy-conversion control; sortedness and NaN-free samples remain preconditions");
  return Report();
}
