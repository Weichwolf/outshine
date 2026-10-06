#include "StructurePlanSelection.h"
#include "PlanHierarchy.h"
#include "Geodesy.h"
#include "math/Units.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <expected>
#include <numeric>
#include <span>
#include <vector>

namespace outshine::Generators {
namespace {
#include "FacadeOpeningValues.h"

bool Stopped(const std::atomic_bool *stopping) {
  return stopping != nullptr && stopping->load(std::memory_order_relaxed);
}

double DistanceTo(const StructureMassPlan &mass, const RawTile &raw, const Box &bounds) {
  double distanceM = 0.0;
  if (raw.EyeEcef && !bounds.Empty()) {
    const Vec3 eye = *raw.EyeEcef - raw.AnchorEcef;
    Vec3 offset{};
    for (size_t axis = 0; axis < 3; ++axis) {
      offset[axis] = eye[axis] - std::clamp(eye[axis], bounds.Min[axis], bounds.Max[axis]);
    }
    distanceM = std::hypot(offset[0], offset[1], offset[2]);
  } else {
    const double centreLon = 0.5 * (mass.LowLon + mass.HighLon);
    const double eyeLon = centreLon + std::remainder(raw.Eye.LongitudeDeg - centreLon, kDegPerTurn);
    const double nearLat = std::clamp(raw.Eye.LatitudeDeg, mass.LowLat, mass.HighLat);
    const double nearLon = std::clamp(eyeLon, mass.LowLon, mass.HighLon);
    distanceM =
        std::hypot((nearLat - raw.Eye.LatitudeDeg) * kMPerDegLat,
                   (nearLon - eyeLon) * kMPerDegLon * std::cos(raw.Eye.LatitudeDeg * kDeg2Rad));
  }
  return std::max(distanceM - kStructureEyeDetailGuardM, 1.0);
}

bool Massable(const StructurePlan &plan) {
  return plan.MinimumHeightM == 0.0 && plan.InnerRings.empty();
}

bool AcceptedMass(const StructureMassPlan &mass,
                  const Box &sourceBounds,
                  const RawTile &raw,
                  const StructureMesher &mesher,
                  MeshScratch &scratch) {
  const auto fits = [&](const Box &bounds) {
    const Vec3 span = bounds.Span();
    return raw.Projection.Allows(std::hypot(span[0], span[1], span[2]),
                                 DistanceTo(mass, raw, bounds));
  };
  if (!fits(sourceBounds)) { return false; }
  std::array<double, 8> ring{};
  std::array<double, 4> corners{};
  const auto plan = DescribeStructureMass(mass, raw, ring, corners);
  const auto parentBounds = mesher.SourceEnvelopeBounds(plan, scratch);
  if (!parentBounds) { return false; }
  Box enclosure = sourceBounds;
  enclosure.Cover(*parentBounds);
  return fits(enclosure);
}

}

bool StructureSelectionView::Contains(LongitudeLatitude eye,
                                      ProjectedErrorBudget projection,
                                      std::optional<Vec3> eyeEcef) const noexcept {
  if (Projection != projection || !std::isfinite(Projection.FocalPx) || Projection.FocalPx <= 0.0 ||
      !std::isfinite(Projection.AllowedErrorPx) || Projection.AllowedErrorPx < 0.0 ||
      !std::isfinite(Eye.LongitudeDeg) || !std::isfinite(Eye.LatitudeDeg) ||
      EyeEcef.has_value() != eyeEcef.has_value()) {
    return false;
  }
  if (EyeEcef) {
    const Vec3 offset = *EyeEcef - *eyeEcef;
    const double distanceM = std::hypot(offset[0], offset[1], offset[2]);
    return std::isfinite(distanceM) && distanceM <= kStructureEyeReuseM;
  }
  const Ellipsoid earth{.SemiMajorM = kWgs84A, .Flattening = 1.0 - std::sqrt(1.0 - kWgs84E2)};
  const auto distance =
      GeodesicOn({.LongitudeDeg = Eye.LongitudeDeg, .LatitudeDeg = Eye.LatitudeDeg},
                 {.LongitudeDeg = eye.LongitudeDeg, .LatitudeDeg = eye.LatitudeDeg},
                 earth);
  return distance.Converged && std::isfinite(distance.AlongM) &&
         distance.AlongM <= kStructureEyeReuseM;
}

std::optional<double> StructurePlanSelection::Add(StructurePlan plan,
                                                  StructureMassPlan mass,
                                                  size_t footprint,
                                                  const StructureMesher &mesher,
                                                  MeshScratch &scratch) {
  Source source{.Plan = plan,
                .Mass = mass,
                .Bounds = mesher.SourceEnvelopeBounds(plan, scratch),
                .ShellErrorM = mesher.ShellSurfaceErrorM(plan, scratch),
                .CornerFirst = Corners_.size(),
                .CornerCount = plan.CornerAslM.size(),
                .Footprint = footprint};
  Corners_.insert(Corners_.end(), plan.CornerAslM.begin(), plan.CornerAslM.end());
  source.Plan.CornerAslM = {};
  Sources_.push_back(source);
  return source.ShellErrorM;
}

void StructurePlanSelection::SelectSource(size_t index,
                                          const RawTile &raw,
                                          const StructureMesher &mesher,
                                          MeshScratch &scratch,
                                          BakedTile &out) {
  Source &source = Sources_[index];
  auto mass = source.Mass;
  mass.Level = LevelOfDetail::Massed;
  if (source.Bounds && Massable(source.Plan) &&
      AcceptedMass(mass, *source.Bounds, raw, mesher, scratch)) {
    Commands_.push_back({.Source = index, .Mass = mass});
    out.FootprintDetails[source.Footprint] = LevelOfDetail::Massed;
    ++out.Lumped;
    ++out.Blocks;
    return;
  }
  const double distanceM = DistanceTo(source.Mass, raw, source.Bounds.value_or(Box{}));
  source.Plan.Coarseness =
      source.Bounds && source.ShellErrorM && std::isfinite(*source.ShellErrorM) &&
              *source.ShellErrorM > 0.0 && raw.Projection.Allows(*source.ShellErrorM, distanceM)
          ? LevelOfDetail::Shell
          : LevelOfDetail::Fine;
  source.Plan.RecessedOpenings = !raw.Projection.Allows(kOpeningDepthM, distanceM);
  out.FootprintDetails[source.Footprint] = source.Plan.Coarseness;
  Commands_.push_back({.Source = index, .Mass = std::nullopt});
}

std::expected<void, StructureBakeError>
StructurePlanSelection::SelectCell(std::span<const size_t> indices,
                                   const RawTile &raw,
                                   const StructureMesher &mesher,
                                   MeshScratch &scratch,
                                   BakedTile &out,
                                   const std::atomic_bool *stopping) {
  std::vector<size_t> known;
  std::vector<Box> bounds;
  std::vector<StructureMassPlan> masses;
  known.reserve(indices.size());
  bounds.reserve(indices.size());
  masses.reserve(indices.size());
  for (const size_t index : indices) {
    if (Stopped(stopping)) { return std::unexpected(StructureBakeErrorKind::Cancelled); }
    const Source &source = Sources_[index];
    if (!source.Bounds) {
      SelectSource(index, raw, mesher, scratch, out);
      continue;
    }
    known.push_back(index);
    bounds.push_back(*source.Bounds);
    masses.push_back(Sources_[index].Mass);
  }
  auto hierarchy = PlanHierarchy::Build(bounds);
  if (!hierarchy) { return std::unexpected(StructureMeshError::InvalidPlan); }
  std::optional<StructureMassPlan> accepted;
  hierarchy->SelectNodes(
      [&](const PlanHierarchy::Node &node) {
        if (Stopped(stopping)) { return true; }
        const auto members = hierarchy->Members(node);
        for (const uint32_t member : members) {
          if (!Massable(Sources_[known[member]].Plan)) { return false; }
        }
        auto mass = CombineStructureMasses(masses, members);
        mass.Level = LevelOfDetail::Massed;
        if (!AcceptedMass(mass, node.Bounds, raw, mesher, scratch)) { return false; }
        accepted = mass;
        return true;
      },
      [&](const PlanHierarchy::Node &node) {
        if (Stopped(stopping)) { return; }
        if (node.Count == 1) {
          SelectSource(known[hierarchy->Members(node).front()], raw, mesher, scratch, out);
          return;
        }
        Commands_.push_back({.Source = 0, .Mass = accepted});
        for (const uint32_t member : hierarchy->Members(node)) {
          out.FootprintDetails[Sources_[known[member]].Footprint] = LevelOfDetail::Massed;
        }
        out.Lumped += static_cast<int>(node.Count);
        ++out.Blocks;
      });
  if (Stopped(stopping)) { return std::unexpected(StructureBakeErrorKind::Cancelled); }
  return {};
}

std::expected<void, StructureBakeError>
StructurePlanSelection::Select(const RawTile &raw,
                               const StructureMesher &mesher,
                               MeshScratch &scratch,
                               BakedTile &out,
                               const std::atomic_bool *stopping) {
  std::vector<size_t> indices(Sources_.size());
  std::ranges::iota(indices, size_t{0});
  std::ranges::sort(indices, [&](size_t left, size_t right) {
    return Sources_[left].Mass.Cell < Sources_[right].Mass.Cell;
  });
  for (size_t first = 0; first < indices.size();) {
    if (Stopped(stopping)) { return std::unexpected(StructureBakeErrorKind::Cancelled); }
    size_t end = first + 1;
    while (end < indices.size() &&
           Sources_[indices[end]].Mass.Cell == Sources_[indices[first]].Mass.Cell) {
      ++end;
    }
    const auto selected = SelectCell(
        std::span(indices).subspan(first, end - first), raw, mesher, scratch, out, stopping);
    if (!selected) { return std::unexpected(selected.error()); }
    first = end;
  }

  return {};
}

std::expected<bool, StructureBakeError>
StructurePlanSelection::Emit(const RawTile &raw,
                             const StructureMesher &mesher,
                             MeshScratch &scratch,
                             BakedTile &out,
                             size_t limit,
                             const std::atomic_bool *stopping) {
  const size_t until = Next_ + std::min(limit, Commands_.size() - Next_);
  for (; Next_ < until; ++Next_) {
    if (Stopped(stopping)) { return std::unexpected(StructureBakeErrorKind::Cancelled); }
    const auto &command = Commands_[Next_];
    std::array<double, 8> ring{};
    std::array<double, 4> corners{};
    StructurePlan plan;
    if (command.Mass) {
      plan = DescribeStructureMass(*command.Mass, raw, ring, corners);
    } else {
      const auto &source = Sources_[command.Source];
      plan = source.Plan;
      plan.CornerAslM = std::span(Corners_).subspan(source.CornerFirst, source.CornerCount);
    }
    const auto emitted = mesher.Mesh(plan, scratch, out.Built);
    if (!emitted) {
      if (emitted.error() != StructureMeshError::UnsupportedFootprint) {
        return std::unexpected(emitted.error());
      }
      ++out.UnsupportedMeshes;
    }
  }
  return Next_ == Commands_.size();
}

}
