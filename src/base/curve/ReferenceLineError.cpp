#include "ReferenceLine.h"
#include "Gauss8.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>

namespace outshine {
namespace {
double Up(double value) noexcept {
  return value == 0 ? 0 : std::nextafter(value, std::numeric_limits<double>::infinity());
}

double Product(double a, double b) noexcept {
  return a == 0 || b == 0 ? 0 : std::nextafter(a * b, std::numeric_limits<double>::infinity());
}

double SpiralTruncation(const Segment &segment, double distance) noexcept {
  if (distance == 0) { return 0; }
  const double rate = (segment.ExitCurvature - segment.EntryCurvature) / segment.LengthM;
  if (rate == 0 && segment.EntryCurvature == 0) { return 0; }
  const double infinity = std::numeric_limits<double>::infinity();
  const double turn = rate * distance;
  const double lower =
      std::nextafter(segment.EntryCurvature + std::nextafter(turn, -infinity), -infinity);
  const double upper =
      std::nextafter(segment.EntryCurvature + std::nextafter(turn, infinity), infinity);
  const double curvature =
      std::max({std::abs(segment.EntryCurvature), std::abs(lower), std::abs(upper)});
  const double a = Product(curvature, distance);
  const double b = Product(Product(Up(std::abs(rate)), distance), distance);
  double previous = 1;
  double current = a;
  for (size_t order = 1; order < 2 * Gauss8::Nodes.size(); ++order) {
    const double next =
        Up(Product(a, current) + Product(Product(static_cast<double>(order), b), previous));
    previous = current;
    current = next;
    if (!std::isfinite(current)) { return std::numeric_limits<double>::infinity(); }
  }
  return Product(Product(distance, Gauss8::RemainderConstant), current);
}
}

std::optional<double> ReferenceLine::PlanarTruncationBoundM(double fromM,
                                                            double toM) const noexcept {
  if (Laid_.empty() || !std::isfinite(fromM) || !std::isfinite(toM) || fromM < 0 || toM < fromM ||
      toM > Length_) {
    return std::nullopt;
  }
  auto first = std::ranges::upper_bound(Laid_, fromM, {}, &Held::AlongM);
  if (first != Laid_.begin()) { --first; }
  double maximum = 0;
  for (auto at = first; at != Laid_.end() && at->AlongM <= toM; ++at) {
    if (at->Declared.Shape != Curve::Spiral) { continue; }
    const double distance = std::min(toM - at->AlongM, at->Declared.LengthM);
    maximum = std::max(maximum, SpiralTruncation(at->Declared, distance));
  }
  return maximum;
}
}
