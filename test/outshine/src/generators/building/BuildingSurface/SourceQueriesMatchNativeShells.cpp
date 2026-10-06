#include "BuildingMesh.h"
#include "BuildingScratch.h"
#include "BuildingSurface.h"
#include "Check.h"
#include "Geodesy.h"

#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <span>

namespace {

using namespace outshine;

std::optional<double> Closest(const Raised &mesh, const Vec3 &origin, const Vec3 &direction) {
  double nearest = std::numeric_limits<double>::infinity();
  const auto trace = [&](std::span<const StoredVertex> points, std::span<const uint32_t> run) {
    for (size_t at = 0; at < run.size(); at += 3) {
      const auto point = [&](uint32_t index) {
        const auto &p = points[index].pos;
        return Vec3{{p[0], p[1], p[2]}};
      };
      const Vec3 a = point(run[at]);
      const Vec3 ab = point(run[at + 1]) - a, ac = point(run[at + 2]) - a;
      const Vec3 p = Cross(direction, ac);
      const double determinant = Dot(ab, p);
      if (std::abs(determinant) < 1e-10) { continue; }
      const Vec3 offset = origin - a;
      const double u = Dot(offset, p) / determinant;
      const Vec3 q = Cross(offset, ab);
      const double v = Dot(direction, q) / determinant;
      const double along = Dot(ac, q) / determinant;
      if (u >= 0 && v >= 0 && u + v <= 1 && along >= 0) { nearest = std::min(nearest, along); }
    }
  };
  trace(mesh.WallCorners, mesh.WallRun);
  trace(mesh.RoofCorners, mesh.RoofRun);
  return std::isfinite(nearest) ? std::optional(nearest) : std::nullopt;
}

}

int main() {
  using namespace outshine::Generators;
  using namespace outshine::Test;
  const std::array<double, 8> ring{48, 16, 48, 16.0003, 48.0002, 16.0003, 48.0002, 16};
  const std::array<double, 4> ground{300, 301, 302, 301};
  StructurePlan plan{.RingLatLon = ring,
                     .BaseAslM = 300,
                     .SeatAslM = 302,
                     .FootAslM = 300,
                     .CornerAslM = ground,
                     .HeightM = 12,
                     .HeightMeasured = true,
                     .Coarseness = LevelOfDetail::Shell};
  GeoToEcef({.LongitudeDeg = 16, .LatitudeDeg = 48, .HeightM = 300}, plan.AnchorEcef);
  const auto axes = EnuAxesEcef({.LongitudeDeg = 16, .LatitudeDeg = 48});
  BuildingMesh mesher;
  BuildingScratch scratch;
  std::vector<double> cuts;
  for (const double pitch : {0.0, 1.0}) {
    plan.PitchedShare = pitch;
    const auto source = BuildingSurface::Prepare(plan, scratch);
    CHECK(source.has_value(),
          "native source preparation includes real foundation and roof parameters");
    if (!source) { continue; }
    Raised mesh;
    CHECK(mesher.Mesh(plan, scratch, mesh).has_value(),
          "an independent native shell reference meshes");
    size_t covered = 0, wallHits = 0;
    for (int e = -7; e < 32; e += 3) {
      for (int n = -7; n < 32; n += 3) {
        const Vec3 start = axes.East * (e + 0.37) + axes.North * (n + 0.19) + axes.Up * 60.0;
        const Vec3 direction = axes.Up * -1.0 + axes.East * 0.071 + axes.North * 0.053;
        const auto direct = source->Trace({.Origin = start, .Direction = direction}, 0, 100, cuts);
        const auto reference = Closest(mesh, start, direction);
        CHECK(direct.has_value() == reference.has_value(),
              "oblique native roof coverage matches the shell");
        if (!direct || !reference) { continue; }
        ++covered;
        CHECK_NEAR(direct->Along,
                   *reference,
                   0.01,
                   "surface distance m",
                   "analytic roots agree within shell welding and float storage error");
        const auto vertex = source->VertexAt(*direct, start + direction * direct->Along);
        CHECK(std::isfinite(vertex.uv()[0]) && std::isfinite(vertex.uv()[1]),
              "surface queries retain native material coordinates");
      }
    }
    for (int n = -5; n < 28; n += 2) {
      const Vec3 start = axes.East * -20.0 + axes.North * (n + 0.41) + axes.Up * 8.0;
      const auto hit = source->Trace({.Origin = start, .Direction = axes.East}, 0, 100, cuts);
      const auto reference = Closest(mesh, start, axes.East);
      CHECK(hit.has_value() == reference.has_value(), "wall coverage matches terrain-aware shells");
      if (!hit || !reference) { continue; }
      ++wallHits;
      CHECK_NEAR(
          hit->Along, *reference, 0.01, "wall distance m", "wall rays use the original footprint");
      CHECK(hit->Face >= 2, "a lateral source query identifies its native facade");
    }
    CHECK(covered > 20 && wallHits > 5, "the comparison exercises native roofs and walls");
  }
  plan.MinimumHeightM = 5;
  plan.CornerAslM = {};
  const auto raised = BuildingSurface::Prepare(plan, scratch);
  CHECK(raised.has_value(), "a raised source preserves its explicit clearance");
  if (raised) {
    CHECK(!raised->Trace({.Origin = axes.East * -20.0 + axes.North * 8.0 + axes.Up * 2.0,
                          .Direction = axes.East},
                         0,
                         100,
                         cuts),
          "a ray below a raised native building passes through");
  }
  return Report();
}
