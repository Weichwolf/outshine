#include "BuildingMesh.h"
#include "BuildingScratch.h"
#include "BuildingSurface.h"
#include "BuildingWallNormals.h"
#include "Check.h"
#include <cmath>
#include <numbers>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  constexpr double latitude = 48.0;
  constexpr double longitude = 16.0;
  constexpr double metresPerDegree = 111320.0;
  std::vector<double> ring;
  for (int corner = 0; corner < 8; ++corner) {
    const double angle = corner * std::numbers::pi / 4.0;
    ring.push_back(latitude + 4.0 * std::sin(angle) / metresPerDegree);
    ring.push_back(longitude +
                   4.8 * std::cos(angle) /
                       (metresPerDegree * std::cos(latitude * std::numbers::pi / 180.0)));
  }
  StructurePlan plan;
  plan.RingLatLon = ring;
  plan.HeightM = 80;
  plan.HeightMeasured = true;
  plan.PitchedShare = 0;
  plan.Coarseness = LevelOfDetail::Shell;
  BuildingScratch scratch;
  const auto source = BuildingSurface::Prepare(plan, scratch);
  CHECK(source && source->Shapes().size() == 1, "elliptical shaft source prepares");
  if (!source || source->Shapes().empty()) { return Report(); }
  const auto &shape = source->Shapes().front();
  CHECK(HasCurvedShaftWalls(shape), "a mildly elliptical shaft has a continuous mantle");
  const auto &axes = source->Axes();
  const auto world = [&](const Vec3 &local) {
    return source->Origin() + axes.East * local[0] + axes.North * local[1] + axes.Up * local[2];
  };
  for (size_t edge = 0; edge < shape.Ring.size(); ++edge) {
    const auto &p = shape.Ring[edge];
    const auto &q = shape.Ring[(edge + 1) % shape.Ring.size()];
    const double length = std::hypot(q.EastM - p.EastM, q.NorthM - p.NorthM);
    const Vec3 geometric{{(q.NorthM - p.NorthM) / length, (p.EastM - q.EastM) / length, 0}};
    const Vec3 outward = axes.East * geometric[0] + axes.North * geometric[1];
    const EastNorth point{.EastM = 0.75 * p.EastM + 0.25 * q.EastM,
                          .NorthM = 0.75 * p.NorthM + 0.25 * q.NorthM};
    const auto position = world({{point.EastM, point.NorthM, shape.SeatM + shape.FootM + 40}});
    std::vector<double> cuts;
    const auto hit = source->Trace(
        {.Origin = position + outward * 20.0, .Direction = outward * -1.0}, 0, 30, cuts);
    CHECK(hit && hit->Face == edge + 2 && std::abs(hit->Along - 20.0) < 1e-6 &&
              Dot(hit->Normal, outward) > 0.999999,
          "visibility and geometric queries retain the exact polygon plane");
    if (!hit) { continue; }
    const Vec3 np = BuildingWallShadingNormal(shape, p, geometric);
    const Vec3 nq = BuildingWallShadingNormal(shape, q, geometric);
    const Vec3 blend = np * 0.75 + nq * 0.25;
    const Vec3 expectedLocal = blend * (1.0 / std::sqrt(Dot(blend, blend)));
    const Vec3 expected = axes.East * expectedLocal[0] + axes.North * expectedLocal[1];
    const auto vertex = source->VertexAt(*hit, position);
    const auto normal = vertex.norm();
    CHECK(Dot(Vec3{{normal[0], normal[1], normal[2]}}, expected) > 0.99999,
          "projected native surfaces use the same interpolated normals as the near mesh");
  }
  return Report();
}
