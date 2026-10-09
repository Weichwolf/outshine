#include "JunctionFootprint.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <vector>

namespace outshine::Generators {

JunctionFootprint BuildJunctionFootprint(std::span<const RoadGate> gates) {
  JunctionFootprint footprint;
  if (gates.size() < 2) { return footprint; }
  for (const auto &gate : gates) {
    footprint.Centre.EastM += gate.EastM;
    footprint.Centre.NorthM += gate.NorthM;
  }
  footprint.Centre.EastM /= static_cast<double>(gates.size());
  footprint.Centre.NorthM /= static_cast<double>(gates.size());

  struct Corner {
    EastNorth At;
    double AroundRad;
    size_t Order;
  };

  std::vector<Corner> corners;
  corners.reserve(gates.size() * 2);
  for (size_t at = 0; at < gates.size(); ++at) {
    const auto &gate = gates[at];
    for (const double hand : {1.0, -1.0}) {
      const EastNorth position{.EastM = gate.EastM + gate.OutN * gate.HalfWidthM * hand,
                               .NorthM = gate.NorthM - gate.OutE * gate.HalfWidthM * hand};
      corners.push_back({.At = position,
                         .AroundRad = std::atan2(footprint.Centre.NorthM - position.NorthM,
                                                 position.EastM - footprint.Centre.EastM),
                         .Order = at * 2 + (hand > 0.0 ? 0u : 1u)});
    }
  }
  std::ranges::sort(corners, [](const auto &a, const auto &b) {
    return a.AroundRad != b.AroundRad ? a.AroundRad < b.AroundRad : a.Order < b.Order;
  });
  footprint.Rim.reserve(corners.size());
  for (const auto &corner : corners) { footprint.Rim.push_back(corner.At); }
  return footprint;
}

}
