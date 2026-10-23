#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__)
#error "Receiver block bounds require strict floating-point arithmetic"
#endif
#include "Projection.h"
#include "Outward.h"
#include <algorithm>
#include <array>
#include <cfenv>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <span>
#include <utility>

namespace ReceiverBlocks {
namespace {
constexpr double Infinity = std::numeric_limits<double>::infinity();
constexpr double Unit = 0x1p-53;
constexpr double Eta = std::numeric_limits<double>::denorm_min();
static_assert(std::numeric_limits<double>::digits == Binary64Precision);

bool Valid(Interval a) noexcept {
  return std::isfinite(a.Lo) && std::isfinite(a.Hi) && a.Lo <= a.Hi;
}

Interval Invalid() noexcept {
  return {.Lo = -Infinity, .Hi = Infinity};
}

Interval Out(double lo, double hi) noexcept {
  if (!std::isfinite(lo) || !std::isfinite(hi)) { return Invalid(); }
  return {.Lo = NextFinite(lo, false), .Hi = NextFinite(hi, true)};
}

Interval Add(Interval a, Interval b) noexcept {
  if (!Valid(a) || !Valid(b)) { return Invalid(); }
  return Out(a.Lo + b.Lo, a.Hi + b.Hi);
}

Interval Scale(Interval a, double coefficient) noexcept {
  if (coefficient == 0) { return {.Lo = 0, .Hi = 0}; }
  if (!Valid(a)) { return Invalid(); }
  const double x = a.Lo * coefficient;
  const double y = a.Hi * coefficient;
  return Out(std::min(x, y), std::max(x, y));
}

Interval Multiply(Interval a, Interval b) noexcept {
  if (!Valid(a) || !Valid(b)) { return Invalid(); }
  const std::array products{a.Lo * b.Lo, a.Lo * b.Hi, a.Hi * b.Lo, a.Hi * b.Hi};
  if (!std::ranges::all_of(products, [](double v) { return std::isfinite(v); })) {
    return Invalid();
  }
  return Out(std::ranges::min(products), std::ranges::max(products));
}

Interval Divide(Interval a, Interval b) noexcept {
  if (!Valid(a) || !Valid(b) || b.Lo <= 0) { return Invalid(); }
  const double lo = a.Lo < 0 ? a.Lo / b.Lo : a.Lo / b.Hi;
  const double hi = a.Hi >= 0 ? a.Hi / b.Lo : a.Hi / b.Hi;
  return Out(lo, hi);
}

double Up(double value) noexcept {
  return std::nextafter(value, Infinity);
}

double Magnitude(Interval a) noexcept {
  return std::max(std::abs(a.Lo), std::abs(a.Hi));
}

double Rounding(Interval a) noexcept {
  return Up(Up(Unit * Magnitude(a)) + Eta);
}

struct Value {
  Interval Range;
  double Error = 0;
};

Value Sum(Value a, Value b) noexcept {
  const auto range = Add(a.Range, b.Range);
  return {.Range = range, .Error = Up(Up(a.Error + b.Error) + Rounding(range))};
}

Value Product(Value a, double coefficient) noexcept {
  if (coefficient == 0) { return {.Range = {.Lo = 0, .Hi = 0}}; }
  const auto range = Scale(a.Range, coefficient);
  return {.Range = range, .Error = Up(Up(std::abs(coefficient) * a.Error) + Rounding(range))};
}

Value Quotient(Value a, Value b) noexcept {
  const auto range = Divide(a.Range, b.Range);
  if (!Valid(range)) { return {.Range = range, .Error = Infinity}; }
  const double error = Up(Up(a.Error + Up(Magnitude(range) * b.Error)) / b.Range.Lo);
  return {.Range = range, .Error = Up(error + Rounding(range))};
}

std::array<Value, 4> Transform(const Matrix &m, const std::array<Value, 3> &point) noexcept {
  std::array<Value, 4> result{};
  for (size_t row = 0; row < result.size(); ++row) {
    Value value{.Range = {.Lo = m[12 + row], .Hi = m[12 + row]}};
    for (size_t column = 0; column < point.size(); ++column) {
      const double coefficient = m[4 * column + row];
      if (coefficient != 0) { value = Sum(value, Product(point[column], coefficient)); }
    }
    result[row] = value;
  }
  return result;
}

std::array<Interval, 4> Coefficients(const Matrix &inverse,
                                     const std::array<double, 3> &eye,
                                     const Matrix &light,
                                     size_t row) noexcept {
  Interval shift{.Lo = light[12 + row], .Hi = light[12 + row]};
  for (size_t i = 0; i < eye.size(); ++i) {
    if (light[4 * i + row] != 0) {
      shift = Add(shift, Scale({.Lo = eye[i], .Hi = eye[i]}, -light[4 * i + row]));
    }
  }
  std::array<Interval, 4> result{};
  for (size_t column = 0; column < result.size(); ++column) {
    auto value = Scale(shift, inverse[4 * column + 3]);
    for (size_t i = 0; i < eye.size(); ++i) {
      const double coefficient = light[4 * i + row];
      if (coefficient != 0) {
        const double a = inverse[4 * column + i];
        value = Add(value, Scale({.Lo = a, .Hi = a}, coefficient));
      }
    }
    result[column] = value;
  }
  return result;
}

bool Arithmetic() noexcept {
  if (std::fegetround() != FE_TONEAREST) { return false; }
  const volatile double small = std::numeric_limits<double>::min();
  const volatile double half = .5;
  const volatile double tiny = Eta;
  const volatile double one = 1.;
  constexpr double HalfNormal = 0x1p-1023;
  return small * half == HalfNormal && tiny * one == Eta;
}
}

std::optional<Prepared> Prepared::Make(const Matrix &inverse,
                                       const std::array<double, 3> &eye,
                                       std::span<const Matrix> lights,
                                       const Domain &frame) noexcept {
  if (!Arithmetic() || lights.empty() || lights.size() > 4 || !std::ranges::all_of(frame, Valid)) {
    return std::nullopt;
  }
  std::array<Value, 3> input{};
  for (size_t i = 0; i < input.size(); ++i) { input[i].Range = frame[i]; }
  const auto camera = Transform(inverse, input);
  if (!Valid(camera[3].Range) || camera[3].Range.Lo <= 0) { return std::nullopt; }
  std::array<Value, 3> shifted{};
  for (size_t i = 0; i < shifted.size(); ++i) {
    shifted[i] = Sum(Quotient(camera[i], camera[3]), {.Range = {.Lo = -eye[i], .Hi = -eye[i]}});
  }
  if (!std::ranges::all_of(
          camera, [](const Value &v) { return Valid(v.Range) && std::isfinite(v.Error); }) ||
      !std::ranges::all_of(
          shifted, [](const Value &v) { return Valid(v.Range) && std::isfinite(v.Error); })) {
    return std::nullopt;
  }
  Prepared result;
  result.Denominator_ = {inverse[3], inverse[7], inverse[11], inverse[15]};
  result.Frame_ = frame;
  result.Lights_.reserve(lights.size());
  for (const auto &matrix : lights) {
    if (matrix[3] != 0 || matrix[7] != 0 || matrix[11] != 0 || matrix[15] != 1) {
      return std::nullopt;
    }
    const auto light = Transform(matrix, shifted);
    Light held;
    for (size_t axis = 0; axis < held.Error.size(); ++axis) {
      const auto value = Quotient(light[axis], light[3]);
      held.Error[axis] = value.Error;
      held.Numerator[axis] = Coefficients(inverse, eye, matrix, axis);
      if (!std::isfinite(value.Error) || !std::ranges::all_of(held.Numerator[axis], Valid)) {
        return std::nullopt;
      }
    }
    result.Lights_.push_back(held);
  }
  return result;
}

bool Prepared::Project(const Domain &box, std::span<Domain> output) const noexcept {
  if (output.size() != Lights_.size() || !std::ranges::all_of(box, Valid)) { return false; }
  for (size_t i = 0; i < box.size(); ++i) {
    if (box[i].Lo < Frame_[i].Lo || box[i].Hi > Frame_[i].Hi) { return false; }
  }
  auto denominatorXY = Add(Scale(box[0], Denominator_[0]), Scale(box[1], Denominator_[1]));
  denominatorXY = Add(denominatorXY, {.Lo = Denominator_[3], .Hi = Denominator_[3]});
  std::array<Interval, 2> denominators{};
  const std::array z{box[2].Lo, box[2].Hi};
  for (size_t i = 0; i < z.size(); ++i) {
    denominators[i] = Add(denominatorXY, Scale({.Lo = z[i], .Hi = z[i]}, Denominator_[2]));
    if (!Valid(denominators[i]) || denominators[i].Lo <= 0) { return false; }
  }
  for (size_t i = 0; i < Lights_.size(); ++i) {
    for (size_t axis = 0; axis < output[i].size(); ++axis) {
      const auto &coefficients = Lights_[i].Numerator[axis];
      auto xy = Add(coefficients[3], Multiply(coefficients[0], box[0]));
      xy = Add(xy, Multiply(coefficients[1], box[1]));
      double lo = Infinity;
      double hi = -Infinity;
      for (size_t j = 0; j < z.size(); ++j) {
        const auto quotient = Divide(Add(xy, Scale(coefficients[2], z[j])), denominators[j]);
        if (!Valid(quotient)) { return false; }
        lo = std::min(lo, quotient.Lo);
        hi = std::max(hi, quotient.Hi);
      }
      output[i][axis] = Out(lo - Lights_[i].Error[axis], hi + Lights_[i].Error[axis]);
      if (!Valid(output[i][axis])) { return false; }
    }
  }
  return true;
}
}
