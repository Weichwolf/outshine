#include "Envelope.h"
#include "Input.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cfenv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>

namespace Projection {
namespace {
constexpr uint32_t Terms = 20;
constexpr int Binary32Digits = 24;
constexpr int Binary64Digits = 53;

struct ErrorLimits {
  double Noise;
  double Gamma;
};

constexpr uint32_t Degree = 3;
constexpr uint32_t Leaves = Terms * Degree;
constexpr uint32_t Operations = Leaves - 1;
constexpr uint32_t Path = Terms + Degree - 2;
constexpr uint32_t NoisyLeaves = Leaves + 3 * Operations;
constexpr uint64_t NoiseTerms = uint64_t{NoisyLeaves + 1} * (NoisyLeaves + 1) * (NoisyLeaves + 1);
constexpr double Unit = 0x1p-23;
constexpr double Tiny = 0x1p-126;
constexpr uint32_t ExponentMask = 0x7f800000;
constexpr uint32_t FractionMask = 0x007fffff;
constexpr uint32_t Binary32One = 0x3f800000;
static_assert(sizeof(float) == 4 && std::numeric_limits<float>::digits == Binary32Digits);
static_assert(std::bit_cast<uint32_t>(1.0f) == Binary32One);
static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<double>::is_iec559);
static_assert(std::numeric_limits<double>::digits == Binary64Digits);

double Up(double value, EnvelopeCosts &work) noexcept {
  if (value == 0) { return value; }
  ++work.OutwardSteps;
  return std::nextafter(value, std::numeric_limits<double>::infinity());
}

double Down(double value, EnvelopeCosts &work) noexcept {
  if (value == 0) { return value; }
  ++work.OutwardSteps;
  return std::nextafter(value, -std::numeric_limits<double>::infinity());
}

double Product(double a, double b, EnvelopeCosts &work) noexcept {
  ++work.Products;
  return a * b;
}

double Sum(double a, double b, EnvelopeCosts &work) noexcept {
  ++work.Sums;
  return a + b;
}

std::expected<double, EnvelopeUnknown> Magnitude(const Input &input, EnvelopeCosts &work) noexcept {
  double maximum = 1;
  const auto add = [&](float value) -> std::expected<void, EnvelopeUnknown> {
    ++work.Inputs;
    const auto bits = std::bit_cast<uint32_t>(value);
    if ((bits & ExponentMask) == ExponentMask) {
      return std::unexpected(EnvelopeUnknown::InputNonfinite);
    }
    if ((bits & ExponentMask) == 0 && (bits & FractionMask) != 0) {
      return std::unexpected(EnvelopeUnknown::InputSubnormal);
    }
    maximum = std::max(maximum, std::abs(static_cast<double>(value)));
    return {};
  };
  for (const float value : input.Model) {
    if (auto result = add(value); !result) { return std::unexpected(result.error()); }
  }
  for (const float value : input.View) {
    if (auto result = add(value); !result) { return std::unexpected(result.error()); }
  }
  for (size_t i = 0; i < 3; ++i) {
    if (auto result = add(input.Shift[i]); !result) { return std::unexpected(result.error()); }
    if (auto result = add(input.Point[i]); !result) { return std::unexpected(result.error()); }
  }
  return maximum;
}

void Accumulate(Interval term, Interval &centre, double &mass, EnvelopeCosts &work) noexcept {
  centre.Lower = Down(Sum(centre.Lower, term.Lower, work), work);
  centre.Upper = Up(Sum(centre.Upper, term.Upper, work), work);
  mass = Up(Sum(mass, std::max(std::abs(term.Lower), std::abs(term.Upper)), work), work);
}

Interval Coordinate(const Input &input,
                    size_t row,
                    const ErrorLimits &limits,
                    EnvelopeCosts &work) noexcept {
  const std::array<double, 4> point{input.Point[0], input.Point[1], input.Point[2], 1};
  Interval centre;
  double mass = 0;
  for (size_t j = 0; j < 4; ++j) {
    for (size_t k = 0; k < 4; ++k) {
      const auto first = Product(input.Model[k * 4 + j], point[k], work);
      const auto value = Product(first, input.View[j * 4 + row], work);
      Accumulate({.Lower = Down(value, work), .Upper = Up(value, work)}, centre, mass, work);
    }
    const double shift = j < 3 ? input.Shift[j] : 0;
    const auto value = Product(shift, input.View[j * 4 + row], work);
    Accumulate({.Lower = value, .Upper = value}, centre, mass, work);
  }
  const auto radius =
      Up(Sum(Up(Product(limits.Gamma, mass, work), work), limits.Noise, work), work);
  return {.Lower = Down(Sum(centre.Lower, -radius, work), work),
          .Upper = Up(Sum(centre.Upper, radius, work), work)};
}
}

std::expected<Envelope, EnvelopeUnknown> Enclose(const Input &input) noexcept {
  if (std::fegetround() != FE_TONEAREST) { return std::unexpected(EnvelopeUnknown::CpuArithmetic); }
  Envelope result;
  auto &work = result.Work;
  const auto maximum = Magnitude(input, work);
  if (!maximum) { return std::unexpected(maximum.error()); }
  const auto square = Up(Product(*maximum, *maximum, work), work);
  work.Divisions += 2;
  const auto growth = Up(1 / (1 - Operations * Unit), work);
  const auto noise =
      Up(Product(Up(Product(NoiseTerms * Tiny, square, work), work), growth, work), work);
  const auto positive = Up(
      Product(
          Up(Product(Terms, Up(Product(square, *maximum, work), work), work), work), growth, work),
      work);
  if (Up(Sum(positive, noise, work), work) >= std::numeric_limits<float>::max()) {
    return std::unexpected(EnvelopeUnknown::OverflowRisk);
  }
  const auto gamma = Up((Path * Unit) / (1 - Path * Unit), work);
  for (size_t row = 0; row < 4; ++row) {
    result.Clip[row] = Coordinate(input, row, {.Noise = noise, .Gamma = gamma}, work);
  }
  return result;
}

std::expected<std::array<Interval, 3>, EnvelopeUnknown> Divide(const Envelope &envelope) noexcept {
  const auto divisor = envelope.Clip[3];
  if (std::fegetround() != FE_TONEAREST) { return std::unexpected(EnvelopeUnknown::CpuArithmetic); }
  for (const auto &range : envelope.Clip) {
    if (!std::isfinite(range.Lower) || !std::isfinite(range.Upper) || range.Lower > range.Upper) {
      return std::unexpected(EnvelopeUnknown::InputNonfinite);
    }
  }
  if (divisor.Lower <= 0) { return std::unexpected(EnvelopeUnknown::ClipCrossing); }
  std::array<Interval, 3> result;
  for (size_t i = 0; i < result.size(); ++i) {
    const auto value = envelope.Clip[i];
    const std::array candidates{value.Lower / divisor.Lower,
                                value.Lower / divisor.Upper,
                                value.Upper / divisor.Lower,
                                value.Upper / divisor.Upper};
    if (!std::ranges::all_of(candidates, [](double v) { return std::isfinite(v); })) {
      return std::unexpected(EnvelopeUnknown::OverflowRisk);
    }
    result[i] = {.Lower = std::nextafter(*std::ranges::min_element(candidates),
                                         -std::numeric_limits<double>::infinity()),
                 .Upper = std::nextafter(*std::ranges::max_element(candidates),
                                         std::numeric_limits<double>::infinity())};
  }
  return result;
}
}
