#include "Check.h"
#include "PlanarStationMesh.h"
#include "SurfaceHeightTopology.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;

void CheckLevels(const SurfaceHeightTopology &topology, bool joined) {
  const std::vector<uint32_t> ids{0, 1, 2, 3, 4, 5};
  CHECK(topology.Sources.size() == 3 && topology.Points.size() == (joined ? 20u : 21u),
        "three coincident planar levels retain independent source-specific heights");
  if (topology.Sources.size() != 3) { return; }
  for (const auto &source : topology.Sources) {
    CHECK(source.PlanarFaces == ids,
          "six distinct refined planar subfaces remain visible to every overlapping source");
    CHECK(source.Faces.size() == 6, "no child is overwritten by a repeated original parent ID");
    if (source.Faces.size() != 6) { return; }
  }
  bool distinctContact = false;
  for (size_t at = 0; at < ids.size(); ++at) {
    const auto lower = topology.Sources[0].Faces[at];
    const auto upper = topology.Sources[2].Faces[at];
    for (size_t corner = 0; corner < 3; ++corner) {
      CHECK(lower[corner] != upper[corner] &&
                topology.Points[lower[corner]] == topology.Points[upper[corner]] &&
                topology.PlanarVertices[lower[corner]] == topology.PlanarVertices[upper[corner]],
            "the same geometric corner has separate height variables on unjoined levels");
    }
    distinctContact |= topology.Sources[0].Faces[at] != topology.Sources[1].Faces[at];
  }
  CHECK(distinctContact, "an admitted endpoint never joins whole overlapping surface layers");
}
}

int main() {
  const std::array points{
      PlanarPoint{0, 0}, PlanarPoint{4, 0}, PlanarPoint{4, 4}, PlanarPoint{0, 4}};
  const std::array faces{PlanarFace{0, 1, 2}, PlanarFace{0, 2, 3}};
  const std::array axes{PlanarStationAxis{.Direction = {1, 0}, .Cuts = {2}}};
  const auto cuts = CutPlanarStations(points, faces, axes);
  CHECK(cuts && cuts->Mesh.Faces.size() == 6,
        "station cuts prepare six common planar children from two original faces");
  if (!cuts) { return Report(); }
  PlanarArrangement refined;
  refined.Mesh = cuts->Mesh;
  const std::array<uint64_t, 2> owners{7, 7};
  for (const auto parent : refined.Mesh.FaceParents) {
    CHECK(parent < 2, "refined faces retain their original source-mask parent");
    if (parent >= owners.size()) { return Report(); }
    refined.FaceDomains.push_back(owners[parent]);
  }
  const auto separate = BuildSurfaceHeightTopology(refined, 3, {}, 0);
  CHECK(separate.has_value(), "shared station refinement supports separate source heights");
  if (separate) { CheckLevels(*separate, false); }
  const std::array ports{SurfaceHeightPort{
      .Region = {.Rings = {{{-1, -1}, {1, -1}, {1, 1}, {-1, 1}}}}, .Domains = {0, 1}}};
  const auto joined = BuildSurfaceHeightTopology(refined, 3, ports, 0);
  CHECK(joined.has_value(), "the refined arrangement retains qualified endpoint ports");
  if (joined) { CheckLevels(*joined, true); }
  return Report();
}
