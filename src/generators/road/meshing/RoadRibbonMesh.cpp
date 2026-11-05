#include "RoadRibbonMesh.h"
#include "RoadCrossSection.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace outshine::Generators {
namespace {
std::array<Vec3f, kRibbonAcross> PortVertices(const RoadPort &port, bool atEnd) {
  auto positions = RoadPortPositions(port);
  if (atEnd) { std::ranges::reverse(positions); }
  return positions;
}

void Pour(const Ribbon &woven, const Vec3f &wearsLinear, RoadMeshBuffers &into) {
  const auto firstVertex = static_cast<uint32_t>(into.PositionM.size() / 3u);
  const size_t vertices = woven.PositionM.size() / 3u;
  for (size_t one = 0; one < vertices; ++one) {
    into.PositionM.push_back(static_cast<float>(woven.PositionM[one * 3u] + woven.OriginM[0]));
    into.PositionM.push_back(static_cast<float>(woven.PositionM[one * 3u + 1u] + woven.OriginM[1]));
    into.PositionM.push_back(static_cast<float>(woven.PositionM[one * 3u + 2u] + woven.OriginM[2]));
    into.NormalM.push_back(woven.NormalM[one * 3u]);
    into.NormalM.push_back(woven.NormalM[one * 3u + 1u]);
    into.NormalM.push_back(woven.NormalM[one * 3u + 2u]);
    into.ColourRgba.push_back(wearsLinear[0]);
    into.ColourRgba.push_back(wearsLinear[1]);
    into.ColourRgba.push_back(wearsLinear[2]);
    into.ColourRgba.push_back(1.0f);
  }
  for (const uint32_t one : woven.Index) { into.Index.push_back(firstVertex + one); }
}

struct PortVertex {
  Vec3f Position;
  double DepthM = 0;
  int Facing = 0;
};

struct PortTarget {
  size_t FirstVertex = 0;
  size_t Stations = 0;
  RibbonForm Form = RibbonForm::ClosedShell;
  bool AtEnd = false;
};

void StorePortVertex(RoadMeshBuffers &into, size_t vertex, Vec3f normal, PortVertex at) {
  for (size_t axis = 0; axis < 3; ++axis) {
    into.PositionM[vertex * 3 + axis] =
        static_cast<float>(at.Position[axis] - at.DepthM * normal[axis]);
    if (at.Facing != 0) {
      into.NormalM[vertex * 3 + axis] = static_cast<float>(at.Facing) * normal[axis];
    }
  }
}

void BindRoadPort(const RoadPort &port, PortTarget target, RoadMeshBuffers &into) {
  const bool closed = target.Form == RibbonForm::ClosedShell;
  const size_t stride = closed ? kRibbonAcross * 2 + 4 : kRibbonAcross;
  const auto positions = PortVertices(port, target.AtEnd);
  const auto normal = RoadPortNormal(port.Plane);
  const size_t station = target.FirstVertex + (target.AtEnd ? (target.Stations - 1) * stride : 0);
  for (size_t across = 0; across < kRibbonAcross; ++across) {
    const auto position = positions[across];
    StorePortVertex(into, station + across, normal, {.Position = position, .Facing = 1});
    if (closed) {
      StorePortVertex(into,
                      station + kRibbonAcross + across,
                      normal,
                      {.Position = position, .DepthM = port.Gate.ThicknessM, .Facing = -1});
    }
  }
  if (!closed) { return; }
  for (size_t side = 0; side < 2; ++side) {
    const size_t across = side == 0 ? 0 : kRibbonAcross - 1;
    const auto position = positions[across];
    const size_t wall = station + kRibbonAcross * 2 + side * 2;
    StorePortVertex(into, wall, normal, {.Position = position});
    StorePortVertex(into, wall + 1, normal, {.Position = position, .DepthM = port.Gate.ThicknessM});
  }
  const size_t cap =
      target.FirstVertex + target.Stations * stride + (target.AtEnd ? kRibbonAcross * 2 : 0);
  for (size_t corner = 0; corner < kRibbonAcross * 2; ++corner) {
    for (size_t axis = 0; axis < 3; ++axis) {
      into.PositionM[(cap + corner) * 3 + axis] = into.PositionM[(station + corner) * 3 + axis];
    }
  }
}

void BindRoadPorts(const Ribbon &woven,
                   const RoadSweep &how,
                   size_t firstVertex,
                   RoadMeshBuffers &into) {
  for (size_t end = 0; end < how.EndPorts.size(); ++end) {
    const auto &port = how.EndPorts[end];
    if (!port) { continue; }
    BindRoadPort(*port,
                 {.FirstVertex = firstVertex,
                  .Stations = woven.Stations,
                  .Form = how.Form,
                  .AtEnd = end != 0},
                 into);
  }
}

}

RoadDisplayContacts RoadRibbonContacts(const RoadSweep &surface) {
  RoadDisplayContacts result;
  for (size_t end = 0; end < surface.EndPorts.size(); ++end) {
    const auto &port = surface.EndPorts[end];
    if (port) { result.EndRings[end] = PortVertices(*port, end != 0); }
  }
  return result;
}

void AppendRoadRibbon(const Ribbon &ribbon, const RoadSweep &surface, RoadMeshBuffers &mesh) {
  const size_t firstVertex = mesh.PositionM.size() / 3;
  Pour(ribbon, surface.WearsLinear, mesh);
  BindRoadPorts(ribbon, surface, firstVertex, mesh);
}
}
