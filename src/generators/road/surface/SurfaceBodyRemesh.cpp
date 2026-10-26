#include "SurfaceBodyInternal.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Generators::SurfaceBodies {
Checked<void> Remesh(State &state, Pass pass) {
  const auto groups = GroupFaces(state, pass);
  if (!groups) { return std::unexpected(groups.error()); }
  if (groups->Regions.empty()) { return {}; }
  std::vector<PlanarPoint> xy;
  xy.reserve(state.Output.ReferencePoints.size());
  for (const auto &point : state.Output.ReferencePoints) { xy.push_back({point[0], point[1]}); }
  const auto rebuilt = RemeshPlanarRegions(
      xy, state.Output.ReferenceFaces, groups->Regions, state.Budget.Regions, state.Stop);
  if (!rebuilt) { return std::unexpected(rebuilt.error()); }
  std::vector<PlanarFace> faces;
  std::vector<uint64_t> owners;
  std::vector<int32_t> levels;
  const auto append = [&](PlanarFace face, uint32_t parent) {
    faces.push_back(face);
    owners.push_back(state.Output.FaceSources[parent]);
    if (pass == Pass::Bands) { levels.push_back(state.Output.FaceLevels[parent]); }
  };
  for (const auto face : groups->Passed) { append(state.Output.ReferenceFaces[face], face); }
  for (size_t group = 0; group < rebuilt->Regions.size(); ++group) {
    for (const auto face : rebuilt->Regions[group]) { append(face, groups->Leaders[group]); }
  }
  state.Output.ReferenceFaces = std::move(faces);
  state.Output.FaceSources = std::move(owners);
  state.Output.FaceLevels = std::move(levels);
  if (pass == Pass::Envelope) {
    state.Output.Work.Envelope = rebuilt->Work;
  } else {
    state.Output.Work.Bands = rebuilt->Work;
  }
  return {};
}

void Compact(State &state) {
  auto &output = state.Output;
  std::vector<uint32_t> remap(output.ReferencePoints.size(), kNone);
  for (const auto face : output.ReferenceFaces) {
    for (const auto vertex : face) { remap[vertex] = 0; }
  }
  std::vector<SurfaceHeightPoint> points;
  for (size_t at = 0; at < remap.size(); ++at) {
    if (remap[at] == kNone) { continue; }
    remap[at] = static_cast<uint32_t>(points.size());
    points.push_back(output.ReferencePoints[at]);
  }
  for (auto &face : output.ReferenceFaces) {
    for (auto &vertex : face) { vertex = remap[vertex]; }
  }
  output.ReferencePoints = std::move(points);
}
}
