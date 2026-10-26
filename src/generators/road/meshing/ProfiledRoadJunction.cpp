#include "ProfiledRoadMesher.h"
#include "JunctionFootprint.h"
#include "RoadCrossSection.h"
#include "PolygonTriangulation.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>

namespace outshine::Generators {

namespace {

struct JunctionVertex {
  Vec3f Position;
  Vec3f Normal;
};

void StoreVertex(RoadMeshBuffers &into, JunctionVertex vertex, Vec3f colour) {
  for (size_t axis = 0; axis < 3; ++axis) {
    into.PositionM.push_back(vertex.Position[axis]);
    into.NormalM.push_back(vertex.Normal[axis]);
  }
  into.ColourRgba.insert(into.ColourRgba.end(), {colour[0], colour[1], colour[2], 1});
}

Vec3f SurfaceAt(const JunctionSurfacePoint &at) {
  return {{static_cast<float>(at.EastM),
           static_cast<float>(at.GradeM),
           static_cast<float>(-at.NorthM)}};
}

Vec3f FloorAt(Vec3f position, Vec3f normal, double thicknessM) {
  for (size_t axis = 0; axis < 3; ++axis) {
    position[axis] = static_cast<float>(position[axis] - thicknessM * normal[axis]);
  }
  return position;
}

}

void ProfiledRoadMesher::Junction(std::span<const RoadGate> gates,
                                  RoadPlane plane,
                                  const Vec3f &wearsLinear,
                                  RoadMeshBuffers &into) const {
  const auto footprint = BuildJunctionFootprint(gates, plane);
  if (footprint.Rim.size() < 3) { return; }
  PolygonRing ring;
  ring.reserve(footprint.Rim.size());
  for (const auto &at : footprint.Rim) { ring.push_back({at.EastM, at.NorthM}); }
  const auto triangles = TriangulatePolygon(std::span<const PolygonRing>(&ring, 1));
  if (!triangles) { return; }
  const Vec3f up = RoadPortNormal(plane);
  const Vec3f down = {{-up[0], -up[1], -up[2]}};
  const auto count = static_cast<uint32_t>(footprint.Rim.size());
  const auto top = static_cast<uint32_t>(into.PositionM.size() / 3);
  for (const auto &at : footprint.Rim) {
    StoreVertex(into, {.Position = SurfaceAt(at), .Normal = up}, wearsLinear);
  }
  const auto bottom = static_cast<uint32_t>(into.PositionM.size() / 3);
  for (const auto &at : footprint.Rim) {
    StoreVertex(
        into, {.Position = FloorAt(SurfaceAt(at), up, at.ThicknessM), .Normal = down}, wearsLinear);
  }
  for (size_t at = 0; at < triangles->size(); at += 3) {
    const uint32_t a = (*triangles)[at];
    const uint32_t b = (*triangles)[at + 1];
    const uint32_t c = (*triangles)[at + 2];
    const auto &p = ring[a];
    const auto &q = ring[b];
    const auto &r = ring[c];
    const bool positive = (q[0] - p[0]) * (r[1] - p[1]) - (q[1] - p[1]) * (r[0] - p[0]) > 0;
    into.Index.insert(into.Index.end(),
                      {top + a, top + (positive ? b : c), top + (positive ? c : b)});
    into.Index.insert(into.Index.end(),
                      {bottom + a, bottom + (positive ? c : b), bottom + (positive ? b : c)});
  }
  for (uint32_t at = 0; at < count; ++at) {
    const uint32_t next = (at + 1) % count;
    const Vec3f here = SurfaceAt(footprint.Rim[at]);
    const Vec3f after = SurfaceAt(footprint.Rim[next]);
    const double east = static_cast<double>(after[0]) - here[0];
    const double z = static_cast<double>(after[2]) - here[2];
    const double run = std::hypot(east, z);
    if (!(run > 0)) { continue; }
    const Vec3f outward = {{static_cast<float>(z / run), 0, static_cast<float>(-east / run)}};
    const auto side = static_cast<uint32_t>(into.PositionM.size() / 3);
    StoreVertex(into, {.Position = here, .Normal = outward}, wearsLinear);
    StoreVertex(into,
                {.Position = FloorAt(here, up, footprint.Rim[at].ThicknessM), .Normal = outward},
                wearsLinear);
    StoreVertex(into, {.Position = after, .Normal = outward}, wearsLinear);
    StoreVertex(into,
                {.Position = FloorAt(after, up, footprint.Rim[next].ThicknessM), .Normal = outward},
                wearsLinear);
    into.Index.insert(into.Index.end(), {side, side + 3, side + 1, side, side + 2, side + 3});
  }
}

}
