#include "Check.h"
#include "SurfaceHeightTopology.h"
#include <array>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;

PlanarArrangement ThreeLevels() {
  PlanarArrangement mesh;
  mesh.Mesh.Points = {{0, 0}, {4, 0}, {0, 4}};
  mesh.Mesh.Faces = {{0, 1, 2}};
  mesh.FaceDomains = {7};
  return mesh;
}

SurfaceHeightPort Port(std::vector<uint32_t> domains) {
  return {.Region = {.Rings = {{{-1, -1}, {1, -1}, {1, 1}, {-1, 1}}}},
          .Domains = std::move(domains)};
}

void HighestDomainBit() {
  constexpr uint32_t count = std::numeric_limits<uint64_t>::digits;
  auto mesh = ThreeLevels();
  mesh.FaceDomains[0] = (uint64_t{1} << (count - 1)) | 1;
  const std::array ports{Port({count - 1, 0})};
  const auto joined = BuildSurfaceHeightTopology(mesh, count, ports, 0);
  CHECK(joined && joined->Points.size() == 5 && joined->Sources.size() == count,
        "the highest domain bit remains valid without signed shifts or phantom sources");
  if (joined) {
    const auto vertex = joined->Sources[0].Faces[0][0];
    CHECK(joined->PortVertices == std::vector<std::vector<uint32_t>>{{vertex}},
          "qualified port membership retains the final shared height variable");
    CHECK(joined->PrimaryDomains[vertex] == count - 1,
          "the highest explicitly selected domain can own the shared variable");
  }
}

void PortHolesRemainSeparate() {
  const std::array ports{
      SurfaceHeightPort{.Region = {.Rings = {{{-1, -1}, {5, -1}, {5, 5}, {-1, 5}},
                                             {{-.5, -.5}, {-.5, .5}, {.5, .5}, {.5, -.5}}}},
                        .Domains = {0, 1}}};
  const auto joined = BuildSurfaceHeightTopology(ThreeLevels(), 3, ports, 0);
  CHECK(joined && joined->Points.size() == 7 && joined->Work.Joins == 2,
        "a port hole excludes its vertex while other qualified vertices still join");
  if (joined) {
    const auto a = joined->Sources[0].Faces[0];
    const auto b = joined->Sources[1].Faces[0];
    CHECK((joined->PortVertices == std::vector<std::vector<uint32_t>>{{a[1], a[2]}}),
          "the port height list excludes hole vertices and unqualified source layers");
    CHECK(a[0] != b[0] && a[1] == b[1] && a[2] == b[2],
          "the hole preserves independent source heights rather than filling the whole port box");
  }
}
}

int main() {
  HighestDomainBit();
  PortHolesRemainSeparate();
  const auto mesh = ThreeLevels();
  const auto separate = BuildSurfaceHeightTopology(mesh, 3, {}, 0);
  CHECK(separate && separate->Points.size() == 9 && separate->Sources.size() == 3,
        "coincident source domains keep independent height variables without qualified ports");
  if (!separate) { return Report(); }
  CHECK((separate->Sources[0].Faces[0] == PlanarFace{0, 1, 2} &&
         separate->Sources[1].Faces[0] == PlanarFace{3, 4, 5} &&
         separate->Sources[2].Faces[0] == PlanarFace{6, 7, 8}),
        "source-major variable identities retain independent stacked surfaces");
  const std::array first{Port({1, 0})};
  const auto joined = BuildSurfaceHeightTopology(mesh, 3, first, 0);
  CHECK(joined && joined->Points.size() == 8 && joined->Work.Joins == 1,
        "one endpoint port joins only its two explicitly qualified source identities");
  if (joined) {
    const auto a = joined->Sources[0].Faces[0];
    const auto b = joined->Sources[1].Faces[0];
    const auto c = joined->Sources[2].Faces[0];
    CHECK(a[0] == b[0] && a[0] != c[0] && a[1] != b[1] && a[2] != b[2],
          "unlisted layers and vertices outside the port never join");
    CHECK(joined->PrimaryDomains[a[0]] == 1 && joined->PlanarVertices[a[0]] == 0,
          "explicit port order preserves the established representative/source contract");
  }
  const std::array chain{Port({1, 0}), Port({2, 1})};
  const auto connected = BuildSurfaceHeightTopology(mesh, 3, chain, 0);
  CHECK(connected && connected->Points.size() == 7 && connected->Work.Joins == 2,
        "successive qualified ports propagate their shared height identity transitively");
  if (connected) {
    const auto vertex = connected->Sources[0].Faces[0][0];
    CHECK(vertex == connected->Sources[1].Faces[0][0] &&
              vertex == connected->Sources[2].Faces[0][0] && connected->PrimaryDomains[vertex] == 2,
          "all admitted sources agree on the final representative without cycles");
    CHECK((connected->PortVertices == std::vector<std::vector<uint32_t>>{{vertex}, {vertex}}),
          "later port unions remap earlier port membership to the final representative");
    CHECK(connected->Sources[2].PlanarFaces == std::vector<uint32_t>{0},
          "each source face retains its original shared planar face index");
  }
  return Report();
}
