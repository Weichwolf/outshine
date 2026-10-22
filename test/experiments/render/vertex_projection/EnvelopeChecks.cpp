#include "EnvelopeChecks.h"
#include "Envelope.h"
#include "Input.h"
#include <array>
#include <cstddef>
#include <cfenv>
#include <limits>

namespace Projection {
namespace {
Input Identity() noexcept {
  Input input;
  for (size_t i = 0; i < 4; ++i) { input.Model[i * 5] = input.View[i * 5] = 1; }
  input.Point = {0.5f, -0.5f, 0.5f, 1};
  return input;
}

bool Rejects(const Input &input, EnvelopeUnknown reason) noexcept {
  const auto result = Enclose(input);
  return !result && result.error() == reason;
}

bool Contains(const Envelope &envelope, const std::array<double, 4> &point) noexcept {
  for (size_t i = 0; i < 4; ++i) {
    if (point[i] < envelope.Clip[i].Lower || point[i] > envelope.Clip[i].Upper) { return false; }
  }
  return true;
}
}

bool CheckEnvelopeDomain() noexcept {
  const auto normal = Enclose(Identity());
  if (!normal || !Contains(*normal, {0.5, -0.5, 0.5, 1}) || !Divide(*normal)) { return false; }
  auto input = Identity();
  input.Point[0] = std::numeric_limits<float>::denorm_min();
  if (!Rejects(input, EnvelopeUnknown::InputSubnormal)) { return false; }
  input = Identity();
  input.Model[0] = std::numeric_limits<float>::infinity();
  if (!Rejects(input, EnvelopeUnknown::InputNonfinite)) { return false; }
  input.Model[0] = std::numeric_limits<float>::quiet_NaN();
  if (!Rejects(input, EnvelopeUnknown::InputNonfinite)) { return false; }
  input.Model[0] = std::numeric_limits<float>::max();
  if (!Rejects(input, EnvelopeUnknown::OverflowRisk)) { return false; }
  input = Identity();
  input.Model[0] = input.Point[0] = std::numeric_limits<float>::min();
  const auto tiny = Enclose(input);
  const double exact = static_cast<double>(input.Model[0]) * input.Point[0];
  if (!tiny || !Contains(*tiny, {exact, -0.5, 0.5, 1}) || !Contains(*tiny, {0, -0.5, 0.5, 1})) {
    return false;
  }
  auto crossing = *normal;
  crossing.Clip[3] = {.Lower = -1, .Upper = 1};
  const auto divided = Divide(crossing);
  if (divided || divided.error() != EnvelopeUnknown::ClipCrossing) { return false; }
  crossing.Clip[3] = {.Lower = 1, .Upper = 0};
  if (Divide(crossing)) { return false; }
  const auto before = std::fegetround();
  if (before != FE_TONEAREST || std::fesetround(FE_DOWNWARD) != 0) { return false; }
  const bool rejected = Rejects(Identity(), EnvelopeUnknown::CpuArithmetic);
  return std::fesetround(before) == 0 && rejected;
}
}
