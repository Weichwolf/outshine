#include "Check.h"
#include "PlanarStationMesh.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <set>
#include <stop_token>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;
constexpr std::array kPoints{PlanarPoint{0, 0}, PlanarPoint{10, 0}, PlanarPoint{10, 10}};
constexpr std::array<PlanarFace, 1> kFaces{{{0, 1, 2}}};
constexpr std::array kStation{0., 10., 10.};
constexpr std::array kHeight{3.1, 5.2, 6.3};
constexpr std::array<bool, 3> kAbove{true, false, true};

auto Interval(double begin, double end, bool reversed = false) {
  std::array axes{PlanarStationAxis{.Cuts = {begin}, .Values = kStation},
                  PlanarStationAxis{.Cuts = {end}, .Values = kStation},
                  PlanarStationAxis{.Cuts = {}, .Values = kHeight}};
  auto above = kAbove;
  if (reversed) {
    std::swap(axes[0], axes[1]);
    std::swap(above[0], above[1]);
  }
  return ClipPlanarStations(kPoints, kFaces, axes, above);
}

using Seam = std::set<std::array<double, 5>>;

Seam CutPoints(const PlanarStationMesh &mesh, double cut) {
  Seam points;
  for (const auto face : mesh.Mesh.Faces) {
    for (const auto vertex : face) {
      const auto &point = mesh.Mesh.Points[vertex];
      if (mesh.Stations[0][vertex] != cut && mesh.Stations[1][vertex] != cut) { continue; }
      const auto root = mesh.SourceEdges[vertex];
      points.insert({point[0],
                     point[1],
                     mesh.Stations[2][vertex],
                     static_cast<double>(root[0]),
                     static_cast<double>(root[1])});
    }
  }
  return points;
}

double Area(const PlanarStationMesh &mesh) {
  double area = 0;
  for (size_t at = 0; at < mesh.Mesh.Faces.size(); ++at) {
    const auto face = mesh.Mesh.Faces[at];
    const auto a = mesh.Mesh.Points[face[0]];
    const auto b = mesh.Mesh.Points[face[1]];
    const auto c = mesh.Mesh.Points[face[2]];
    const double twice = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
    CHECK(twice > 0 && mesh.Mesh.FaceParents[at] == 0,
          "all clipped faces preserve positive winding and the actual original triangle");
    area += twice / 2;
  }
  return area;
}

void CommonBoundary() {
  const auto left = Interval(1.3, 7.7);
  const auto right = Interval(7.7, 9.1);
  const auto reverse = Interval(1.3, 7.7, true);
  CHECK(left && right && reverse, "both independently clipped neighbors and reverse order build");
  if (!left || !right || !reverse) { return; }
  const auto seam = CutPoints(*left, 7.7);
  CHECK(
      seam.size() == 2 && seam == CutPoints(*right, 7.7) && seam == CutPoints(*reverse, 7.7),
      "the shared cut has exactly its two original-edge intersections, with bit-identical height");
  CHECK(std::abs(Area(*left) + Area(*right) - ((9.1 * 9.1 - 1.3 * 1.3) / 2)) < 1e-13,
        "adjacent intervals preserve their complete footprint without overlap or missing area");
  for (const auto &point : seam) {
    CHECK(point[0] == 7.7 && point[3] < 3 && point[4] < 3,
          "shared cut points retain original input edge identity instead of a transient diagonal");
  }
}

void MultipleCutsAndRefusals() {
  std::array axes{PlanarStationAxis{.Cuts = {1.3, 7.7}, .Values = kStation}};
  constexpr std::array above{true};
  const auto clipped = ClipPlanarStations(kPoints, kFaces, axes, above);
  CHECK(clipped && std::abs(Area(*clipped) - (50 - 7.7 * 7.7 / 2)) < 1e-13,
        "different cuts on one scalar field use distinct original-edge intersections");
  if (clipped) {
    for (const auto face : clipped->Mesh.Faces) {
      for (const auto vertex : face) {
        CHECK(clipped->Stations[0][vertex] >= 7.7, "every output vertex satisfies both halfspaces");
      }
    }
  }
  const auto capped = ClipPlanarStations(kPoints, kFaces, axes, above, {.Vertices = 3});
  CHECK(!capped && capped.error() == PlanarMeshError::CapacityExceeded,
        "a vertex cap refuses the whole result rather than publishing partial coverage");
  axes[0].Values = std::span(kStation).first(2);
  const auto shortValues = ClipPlanarStations(kPoints, kFaces, axes, above);
  CHECK(!shortValues && shortValues.error() == PlanarMeshError::InvalidInput,
        "explicit scalar fields must contain exactly one value per original point");
  auto bad = kStation;
  bad[1] = std::numeric_limits<double>::infinity();
  axes[0].Values = bad;
  CHECK(!ClipPlanarStations(kPoints, kFaces, axes, above), "nonfinite scalar input is refused");
  std::stop_source stop;
  stop.request_stop();
  const auto canceled = ClipPlanarStations(kPoints, kFaces, axes, above, {}, stop.get_token());
  CHECK(!canceled && canceled.error() == PlanarMeshError::Canceled,
        "cancellation is checked before input copying and returns no mesh");
}
}

int main() {
  CommonBoundary();
  MultipleCutsAndRefusals();
  return Report();
}
