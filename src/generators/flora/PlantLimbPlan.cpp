#include "PlantLimbPlan.h"
#include "math/Vec3.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>

namespace outshine::Generators {
namespace {
constexpr double kLeastSpeedM = 1e-12;

bool Finite(Vec3 value) {
  return std::ranges::all_of(value, [](double component) { return std::isfinite(component); });
}

double EndpointSlope(double first, double second) {
  const double slope = (3 * first - second) / 2;
  if (std::signbit(slope) != std::signbit(first) || first == 0) { return 0; }
  if (std::signbit(first) != std::signbit(second) && std::abs(slope) > 3 * std::abs(first)) {
    return 3 * first;
  }
  return slope;
}

double RadiusSlope(std::span<const PlantLimbAnchor> anchors, size_t at) {
  const auto delta = [&](size_t index) {
    return anchors[index + 1].RadiusM - anchors[index].RadiusM;
  };
  if (anchors.size() == 2) { return delta(0); }
  if (at == 0) { return EndpointSlope(delta(0), delta(1)); }
  if (at + 1 == anchors.size()) { return EndpointSlope(delta(at - 1), delta(at - 2)); }
  const double before = delta(at - 1);
  const double after = delta(at);
  if (before == 0 || after == 0 || std::signbit(before) != std::signbit(after)) { return 0; }
  return 2 / (1 / before + 1 / after);
}

Vec3 DirectionAt(std::span<const PlantLimbAnchor> anchors, size_t at) {
  if (at == 0) { return anchors[1].PositionM - anchors[0].PositionM; }
  if (at + 1 == anchors.size()) { return anchors[at].PositionM - anchors[at - 1].PositionM; }
  const Vec3 before = anchors[at].PositionM - anchors[at - 1].PositionM;
  const Vec3 after = anchors[at + 1].PositionM - anchors[at].PositionM;
  const double left = Length(before);
  const double right = Length(after);
  const Vec3 bisector = before * (1 / left) + after * (1 / right);
  return bisector * (std::min(left, right) / Length(bisector));
}

bool Valid(std::span<const PlantLimbAnchor> anchors) {
  if (anchors.size() < 2 || anchors.size() > PlantLimbPlan::MostAnchors) { return false; }
  std::array<uint32_t, PlantLimbPlan::MostAnchors> nodes{};
  for (size_t at = 0; at < anchors.size(); ++at) {
    const auto &anchor = anchors[at];
    if (!Finite(anchor.PositionM) || !(anchor.RadiusM > 0) || !std::isfinite(anchor.RadiusM)) {
      return false;
    }
    if (at > 0) {
      const double distance = Length(anchor.PositionM - anchors[at - 1].PositionM);
      if (!(distance > 0) || !std::isfinite(distance)) { return false; }
    }
    nodes[at] = anchor.Node;
  }
  const auto selected = std::span(nodes).first(anchors.size());
  std::ranges::sort(selected);
  return std::ranges::adjacent_find(selected) == selected.end();
}
}

std::expected<PlantLimbPlan, std::string_view>
PlantLimbPlan::Create(std::span<const PlantLimbAnchor> anchors) {
  if (!Valid(anchors)) { return std::unexpected("bounded finite unique limb anchors required"); }
  PlantLimbPlan plan;
  plan.Nodes_.reserve(anchors.size());
  plan.Segments_.reserve(anchors.size() - 1);
  const auto count = static_cast<double>(anchors.size() - 1);
  const double step = 1 / count;
  for (size_t at = 0; at < anchors.size(); ++at) {
    plan.Nodes_.push_back(anchors[at].Node);
    if (at + 1 == anchors.size()) { break; }
    const auto &start = anchors[at];
    const auto &end = anchors[at + 1];
    const double parameter = static_cast<double>(at) * step;
    const double span =
        (at + 2 == anchors.size() ? 1 : static_cast<double>(at + 1) * step) - parameter;
    Segment segment{.Parameter = parameter,
                    .Span = span,
                    .Controls = {start.PositionM,
                                 start.PositionM + DirectionAt(anchors, at) * (count * span / 3),
                                 end.PositionM - DirectionAt(anchors, at + 1) * (count * span / 3),
                                 end.PositionM},
                    .Radius = {}};
    const Vec3 guide =
        (end.PositionM - start.PositionM) * (1 / Length(end.PositionM - start.PositionM));
    for (size_t control = 0; control < 3; ++control) {
      const Vec3 velocity =
          (segment.Controls[control + 1] - segment.Controls[control]) * (3 * count);
      if (!Finite(velocity) || !(Dot(velocity, guide) > kLeastSpeedM)) {
        return std::unexpected("forward regular cubic limb controls required");
      }
    }
    const double delta = end.RadiusM - start.RadiusM;
    const double left = RadiusSlope(anchors, at);
    const double right = RadiusSlope(anchors, at + 1);
    segment.Radius = {start.RadiusM, left, 3 * delta - 2 * left - right, left + right - 2 * delta};
    if (!std::ranges::all_of(segment.Radius, [](double value) { return std::isfinite(value); })) {
      return std::unexpected("finite limb radius coefficients required");
    }
    plan.Segments_.push_back(segment);
  }
  return plan;
}

std::expected<PlantLimbSample, std::string_view>
PlantLimbPlan::Sample(double parameter) const noexcept {
  if (!std::isfinite(parameter) || parameter < 0 || parameter > 1 || Segments_.empty()) {
    return std::unexpected("finite normalized limb parameter required");
  }
  const auto after = std::ranges::upper_bound(Segments_, parameter, {}, &Segment::Parameter);
  const auto &segment = *(after - 1);
  const double t = (parameter - segment.Parameter) / segment.Span;
  const double inverse = 1 - t;
  const double scale = 1 / segment.Span;
  const auto &p = segment.Controls;
  const Vec3 velocity = ((p[1] - p[0]) * (inverse * inverse) + (p[2] - p[1]) * (2 * inverse * t) +
                         (p[3] - p[2]) * (t * t)) *
                        (3 * scale);
  const Vec3 acceleration =
      ((p[2] - p[1] * 2.0 + p[0]) * inverse + (p[3] - p[2] * 2.0 + p[1]) * t) * (6 * scale * scale);
  const double speed = Length(velocity);
  if (!(speed > 0) || !std::isfinite(speed)) {
    return std::unexpected("finite limb speed required");
  }
  const Vec3 tangent = velocity * (1 / speed);
  const auto &r = segment.Radius;
  PlantLimbSample sample{
      .CentreM = p[0] * (inverse * inverse * inverse) + p[1] * (3 * inverse * inverse * t) +
                 p[2] * (3 * inverse * t * t) + p[3] * (t * t * t),
      .Tangent = tangent,
      .Curvature = (acceleration - tangent * Dot(acceleration, tangent)) * (1 / speed / speed),
      .SpeedM = speed,
      .RadiusM = ((r[3] * t + r[2]) * t + r[1]) * t + r[0],
      .RadiusSlopeM = ((3 * r[3] * t + 2 * r[2]) * t + r[1]) * scale};
  if (!Finite(sample.CentreM) || !Finite(sample.Curvature) || !(sample.RadiusM > 0) ||
      !std::isfinite(sample.RadiusM) || !std::isfinite(sample.RadiusSlopeM)) {
    return std::unexpected("finite positive limb sample required");
  }
  return sample;
}
}
