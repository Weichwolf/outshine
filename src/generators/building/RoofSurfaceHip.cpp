#include "RoofSurface.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <expected>
#include <limits>
#include <memory>
#include <optional>
#include <vector>

namespace outshine::Generators {
namespace {
EastNorth Local(EastNorth point, EastNorth origin) {
  return {.EastM = point.EastM - origin.EastM, .NorthM = point.NorthM - origin.NorthM};
}

double Cross(EastNorth a, EastNorth b) {
  return a.EastM * b.NorthM - a.NorthM * b.EastM;
}
}

RoofSurface::RoofSurface(const BuildingShape &shape) : Shape_(shape) {
  if (shape.Roof != RoofKind::Hip) { return; }
  Hip_ = shape.HipRoof.get();
  if (Hip_ == nullptr) {
    LocalHip_.emplace(HipRoofPlan::Prepare(shape.Ring, shape.Holes, {}));
    Hip_ = &*LocalHip_;
  }
}

std::expected<void, HipRoofError> RoofSurface::PrepareHip(BuildingShape &shape) {
  if (!shape.HipRoof) {
    shape.HipRoof = std::make_shared<const std::expected<HipRoofPlan, HipRoofError>>(
        HipRoofPlan::Prepare(shape.Ring, shape.Holes, {}));
  }
  if (!*shape.HipRoof) { return std::unexpected(shape.HipRoof->error()); }
  const double rise = std::min(shape.RiseM, kSteepestRoof * (**shape.HipRoof).MaximumDistanceM());
  shape.EavesM += shape.RiseM - rise;
  shape.RiseM = rise;
  return {};
}

double RoofSurface::HipHeightAt(EastNorth point) const noexcept {
  if (Hip_ == nullptr || !*Hip_) { return Shape_.RiseM; }
  const auto &plan = **Hip_;
  return plan.DistanceAt(point).value_or(0.0) * Shape_.RiseM / plan.MaximumDistanceM();
}

std::optional<RoofSurface::Hit> RoofSurface::TraceHip(const Vec3 &origin,
                                                      const Vec3 &direction,
                                                      double minimum,
                                                      double maximum) const noexcept {
  if (Hip_ == nullptr || !*Hip_) {
    if (direction[2] == 0) { return std::nullopt; }
    const double along = (Shape_.RiseM - origin[2]) / direction[2];
    if (along < minimum || along > maximum || !std::isfinite(along) ||
        !Contains({.EastM = origin[0] + along * direction[0],
                   .NorthM = origin[1] + along * direction[1]})) {
      return std::nullopt;
    }
    return Hit{.Along = along, .Normal = {{0, 0, 1}}};
  }
  const auto &plan = **Hip_;
  const double scale = Shape_.RiseM / plan.MaximumDistanceM();
  const auto local = Local({.EastM = origin[0], .NorthM = origin[1]}, plan.Origin());
  std::optional<Hit> hit;
  for (const auto &facet : plan.Facets()) {
    const double divisor = direction[2] - scale * (facet.Gradient.EastM * direction[0] +
                                                   facet.Gradient.NorthM * direction[1]);
    if (divisor == 0) { continue; }
    const double along = (scale * facet.DistanceAt(local) - origin[2]) / divisor;
    if (along < minimum || along > maximum || !std::isfinite(along)) { continue; }
    const EastNorth point{.EastM = local.EastM + along * direction[0],
                          .NorthM = local.NorthM + along * direction[1]};
    if (!facet.Contains(point, plan.PrecisionM())) { continue; }
    const Vec3 normal{{-scale * facet.Gradient.EastM, -scale * facet.Gradient.NorthM, 1}};
    hit = Hit{.Along = along, .Normal = normal * (1 / std::hypot(normal[0], normal[1], normal[2]))};
    maximum = along;
  }
  return hit;
}

void RoofSurface::HipBreaksAlong(EastNorth from, EastNorth to, std::vector<double> &at) const {
  at.clear();
  if (Hip_ == nullptr || !*Hip_) { return; }
  const auto &plan = **Hip_;
  from = Local(from, plan.Origin());
  to = Local(to, plan.Origin());
  const EastNorth ray = Local(to, from);
  const double length = std::hypot(ray.EastM, ray.NorthM);
  if (length == 0) { return; }
  const double tolerance =
      std::max(plan.PrecisionM() / length, 64 * std::numeric_limits<double>::epsilon());
  for (const auto &facet : plan.Facets()) {
    for (size_t edge = 0; edge < facet.Ring.size(); ++edge) {
      const auto a = facet.Ring[edge];
      const auto direction = Local(facet.Ring[(edge + 1) % facet.Ring.size()], a);
      const double divisor = Cross(ray, direction);
      if (divisor == 0) { continue; }
      const auto offset = Local(a, from);
      const double along = Cross(offset, direction) / divisor;
      const double across = Cross(offset, ray) / divisor;
      if (along > tolerance && along < 1 - tolerance && across >= -tolerance &&
          across <= 1 + tolerance && std::isfinite(along)) {
        at.push_back(along);
      }
    }
  }
  std::ranges::sort(at);
  at.erase(std::ranges::unique(at, [&](double a, double b) { return std::abs(a - b) <= tolerance; })
               .begin(),
           at.end());
}

}
