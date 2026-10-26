#include "JunctionFootprint.h"
#include "RoadCrossSection.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <vector>

namespace outshine::Generators {

JunctionFootprint BuildJunctionFootprint(std::span<const RoadGate> gates, RoadPlane plane) {
  JunctionFootprint footprint;
  if (gates.size() < 2) { return footprint; }
  for (const auto &gate : gates) {
    footprint.Centre.EastM += gate.EastM;
    footprint.Centre.NorthM += gate.NorthM;
  }
  footprint.Centre.EastM /= static_cast<double>(gates.size());
  footprint.Centre.NorthM /= static_cast<double>(gates.size());

  struct Corner {
    JunctionSurfacePoint At;
    double AroundRad;
    size_t Order;
  };

  std::vector<Corner> corners;
  corners.reserve(gates.size() * kRibbonAcross);
  for (size_t at = 0; at < gates.size(); ++at) {
    const auto &gate = gates[at];
    const auto positions = RoadPortPositions({.Gate = gate, .Plane = plane});
    for (size_t across = 0; across < positions.size(); ++across) {
      const auto &point = positions[across];
      const JunctionSurfacePoint position{.EastM = point[0],
                                          .NorthM = -point[2],
                                          .GradeM = point[1],
                                          .ThicknessM = gate.ThicknessM};
      corners.push_back({.At = position,
                         .AroundRad = std::atan2(footprint.Centre.NorthM - position.NorthM,
                                                 position.EastM - footprint.Centre.EastM),
                         .Order = at * kRibbonAcross + across});
    }
  }
  std::ranges::sort(corners, [](const auto &a, const auto &b) {
    return a.AroundRad != b.AroundRad ? a.AroundRad < b.AroundRad : a.Order < b.Order;
  });
  footprint.Rim.reserve(corners.size());
  for (const auto &corner : corners) {
    if (!footprint.Rim.empty() && corner.At.EastM == footprint.Rim.back().EastM &&
        corner.At.NorthM == footprint.Rim.back().NorthM) {
      continue;
    }
    footprint.Rim.push_back(corner.At);
  }
  if (footprint.Rim.size() > 1 && footprint.Rim.front().EastM == footprint.Rim.back().EastM &&
      footprint.Rim.front().NorthM == footprint.Rim.back().NorthM) {
    footprint.Rim.pop_back();
  }
  return footprint;
}

}
