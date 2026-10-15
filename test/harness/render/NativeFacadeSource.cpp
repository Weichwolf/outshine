#include "BuildingMesh.h"
#include "BuildingScratch.h"
#include "BuildingSurface.h"
#include "Check.h"
#include "Geodesy.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <print>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;

struct Counts {
  size_t Points = 0, Gables = 0, Blank = 0;
};

Vec3 Global(const EnuAxes &axes, const Vec3 &local) {
  return axes.East * local[0] + axes.North * local[1] + axes.Up * local[2];
}

void Point(const BuildingSurface &source,
           const Vec3 &position,
           const Vec3 &outward,
           size_t scenario,
           double anchorM,
           Counts &counts) {
  std::vector<double> cuts;
  const Vec3 origin = position + outward;
  const Vec3 direction = outward * -1.0;
  const auto hit = source.Trace({.Origin = origin, .Direction = direction}, 0, 2, cuts);
  if (!hit || hit->Face < 2) { return; }
  const auto &shape = source.Shapes()[hit->Part];
  const size_t edge = hit->Face - 2;
  if (edge >= shape.Ring.size()) { return; }
  const auto &a = shape.Ring[edge], &b = shape.Ring[(edge + 1) % shape.Ring.size()];
  const Vec3 at = origin + direction * hit->Along;
  const Vec3 relative = at - source.Origin();
  const Vec3 local{{Dot(relative, source.Axes().East),
                    Dot(relative, source.Axes().North),
                    Dot(relative, source.Axes().Up)}};
  const double e = b.EastM - a.EastM, n = b.NorthM - a.NorthM;
  const double length = std::hypot(e, n);
  const double u = ((local[0] - a.EastM) * e + (local[1] - a.NorthM) * n) / length;
  const double v = local[2] - shape.SeatM - shape.FootM;
  const Vec2f metric{{static_cast<float>(u), static_cast<float>(v)}};
  const auto vertex = source.VertexAt(*hit, at);
  CHECK(std::isfinite(vertex.texture[0]) && std::isfinite(vertex.texture[1]),
        "the actual native source supplies finite facade UVs");
  CHECK(std::abs(static_cast<double>(metric[0]) - u) <= 0.0002 &&
            std::abs(static_cast<double>(metric[1]) - v) <= 0.0002,
        "explicit metre coordinates retain sub-millimetre source precision in these cases");
  ++counts.Points;
  counts.Gables += hit->Gable ? 1u : 0u;
  counts.Blank += std::fmod(vertex.texture[0], 256.0f) == 0.0f ? 1u : 0u;
  std::println("{{\"kind\":\"point\",\"case\":{},\"anchor_m\":{},\"part\":{},"
               "\"face\":{},\"gable\":{},\"uv\":[{:.17g},{:.17g}],"
               "\"metres\":[{:.17g},{:.17g}],\"stored_metres\":[{:.17g},{:.17g}],"
               "\"local\":[{:.17g},{:.17g},{:.17g}],\"base_m\":{:.17g},"
               "\"edge\":[{:.17g},{:.17g},{:.17g},{:.17g}]}}",
               scenario,
               anchorM,
               hit->Part,
               hit->Face,
               hit->Gable,
               vertex.texture[0],
               vertex.texture[1],
               u,
               v,
               metric[0],
               metric[1],
               local[0],
               local[1],
               local[2],
               shape.SeatM + shape.FootM,
               a.EastM,
               a.NorthM,
               b.EastM,
               b.NorthM);
}

void Sample(const BuildingSurface &source, size_t scenario, double anchorM, Counts &counts) {
  for (const auto &shape : source.Shapes()) {
    for (size_t edge = 0; edge < shape.Ring.size(); ++edge) {
      const auto &a = shape.Ring[edge], &b = shape.Ring[(edge + 1) % shape.Ring.size()];
      const double e = b.EastM - a.EastM, n = b.NorthM - a.NorthM;
      const double length = std::hypot(e, n);
      if (length == 0.0) { continue; }
      const Vec3 outward = Global(source.Axes(), {{n / length, -e / length, 0}});
      for (double t : {0.125, 0.5, 0.875}) {
        for (double height : {shape.FloorM * 0.5, shape.EavesM + shape.RiseM * 0.5}) {
          const Vec3 local{{a.EastM + t * e, a.NorthM + t * n, shape.SeatM + shape.FootM + height}};
          Point(source,
                source.Origin() + Global(source.Axes(), local),
                outward,
                scenario,
                anchorM,
                counts);
        }
      }
    }
  }
}

void Inspect(size_t scenario, double latitude, double longitude, double widthDeg) {
  const std::array ring{latitude,
                        longitude,
                        latitude,
                        longitude + widthDeg,
                        latitude + 0.0002,
                        longitude + widthDeg,
                        latitude + 0.0002,
                        longitude};
  BuildingMesh mesher;
  BuildingScratch scratch;
  Counts counts;
  for (const double anchorM : {0.0, 500.0}) {
    StructurePlan plan;
    plan.RingLatLon = ring;
    plan.BaseAslM = 300;
    plan.HeightM = 12;
    plan.HeightMeasured = true;
    plan.PitchedShare = 1;
    GeoToEcef({.LongitudeDeg = longitude, .LatitudeDeg = latitude, .HeightM = 300},
              plan.AnchorEcef);
    const auto axes = EnuAxesEcef({.LongitudeDeg = longitude, .LatitudeDeg = latitude});
    plan.AnchorEcef = plan.AnchorEcef + axes.East * anchorM + axes.North * anchorM;
    const auto source = BuildingSurface::Prepare(plan, scratch);
    CHECK(source.has_value(), "the actual native building source prepares");
    if (!source) { continue; }
    Sample(*source, scenario, anchorM, counts);
    plan.Prepared = &*source;
    for (const auto detail : {LevelOfDetail::Fine,
                              LevelOfDetail::Shell,
                              LevelOfDetail::Massed,
                              LevelOfDetail::Skyline}) {
      plan.Coarseness = detail;
      Raised mesh;
      CHECK(mesher.Mesh(plan, scratch, mesh).has_value(), "all actual native mesh rungs build");
      std::println("{{\"kind\":\"mesh\",\"case\":{},\"anchor_m\":{},\"detail\":{},"
                   "\"wall_vertices\":{},\"used_bytes\":{},\"optional_metric_bytes\":{}}}",
                   scenario,
                   anchorM,
                   static_cast<int>(detail),
                   mesh.WallCorners.size(),
                   mesh.UsedBytes(),
                   mesh.WallCorners.size() * sizeof(Vec2f));
    }
  }
  CHECK(counts.Points > 0, "the source comparison contains actual wall hits");
  std::println("{{\"kind\":\"summary\",\"case\":{},\"points\":{},\"gables\":{},\"blank\":{}}}",
               scenario,
               counts.Points,
               counts.Gables,
               counts.Blank);
}
}

int main() {
  Inspect(0, 48.2, 16.37, 0.0003);
  Inspect(1, 47.0, 9.0, 0.02);
  Inspect(2, 35.68, 139.7, 0.0003);
  Inspect(3, 40.78, -73.96, 0.0003);
  return Report();
}
