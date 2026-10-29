#include "TransportPlanCodec.h"
#include "TransportNetworkSnapshot.h"
#include <algorithm>
#include <cstddef>
#include <limits>
#include <string>
#include <vector>

namespace outshine::Generators {
namespace {
class Capacity {
public:
  template <class T> void Vector(const std::vector<T> &items) noexcept {
    Add(items.capacity() * sizeof(T));
  }

  void Text(const std::string &text) noexcept { Add(text.capacity() + 1); }

  void Add(size_t bytes) noexcept {
    Value += std::min(bytes, std::numeric_limits<size_t>::max() - Value);
  }

  size_t Value = sizeof(TransportPlan);
};
}

size_t TransportPlanCapacityBytes(const TransportPlan &plan) noexcept {
  Capacity held;
  if (plan.Network) { held.Add(plan.Network->CapacityBytes()); }
  held.Vector(plan.Inputs);
  held.Vector(plan.Paths);
  held.Vector(plan.Stations);
  held.Vector(plan.Junctions);
  held.Vector(plan.Ports);
  held.Vector(plan.SourceEnds);
  held.Vector(plan.Bodies);
  held.Vector(plan.TerrainContacts);
  held.Vector(plan.Omissions);
  for (const auto &input : plan.Inputs) {
    held.Text(input.DatasetId);
    held.Text(input.Version);
    held.Text(input.Revision);
    held.Text(input.ItemId);
  }
  for (const auto &path : plan.Paths) {
    held.Text(path.PartId);
    held.Text(path.SourceId);
    held.Text(path.Material);
  }
  for (const auto &junction : plan.Junctions) {
    held.Vector(junction.Sources);
    held.Vector(junction.Gates);
    held.Text(junction.Material);
    for (const auto &source : junction.Sources) { held.Text(source); }
  }
  for (const auto &port : plan.Ports) { held.Text(port.SourceId); }
  for (const auto &end : plan.SourceEnds) {
    held.Text(end.SourceId);
    held.Text(end.Input.DatasetId);
    held.Text(end.Input.Version);
    held.Text(end.Input.Revision);
    held.Text(end.Input.ItemId);
    held.Vector(end.Topology.Relations);
  }
  for (const auto &body : plan.Bodies) {
    held.Vector(body.Points);
    held.Vector(body.Faces);
    held.Vector(body.WallParents);
    held.Vector(body.WallKinds);
  }
  for (const auto &stamp : plan.TerrainContacts) { held.Add(stamp.HeapBytes()); }
  for (const auto &omission : plan.Omissions) { held.Text(omission.SourceId); }
  return held.Value;
}
}
