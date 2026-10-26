#include "PlanarStationMesh.h"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <format>
#include <fstream>
#include <iostream>
#include <limits>
#include <ratio>
#include <utility>
#include <vector>

namespace {
using namespace outshine;

bool ReadMesh(std::istream &input,
              std::vector<PlanarPoint> &points,
              std::vector<PlanarFace> &faces,
              std::vector<uint64_t> &owners) {
  size_t count = 0;
  size_t triangles = 0;
  if (!(input >> count >> triangles) || count > PlanarStationBudget{}.Vertices ||
      triangles > PlanarStationBudget{}.Faces) {
    return false;
  }
  points.resize(count);
  faces.resize(triangles);
  owners.resize(triangles);
  for (auto &point : points) {
    if (!(input >> point[0] >> point[1])) { return false; }
  }
  for (size_t at = 0; at < triangles; ++at) {
    auto &face = faces[at];
    if (!(input >> face[0] >> face[1] >> face[2] >> owners[at])) { return false; }
  }
  return true;
}

bool ReadAxes(std::istream &input, std::vector<PlanarStationAxis> &axes) {
  size_t count = 0;
  if (!(input >> count) || count > PlanarStationBudget{}.Axes) { return false; }
  axes.resize(count);
  size_t total = 0;
  for (auto &axis : axes) {
    size_t cuts = 0;
    if (!(input >> axis.Origin[0] >> axis.Origin[1] >> axis.Direction[0] >> axis.Direction[1] >>
          cuts) ||
        cuts > PlanarStationBudget{}.Cuts - total) {
      return false;
    }
    total += cuts;
    axis.Cuts.resize(cuts);
    for (auto &station : axis.Cuts) {
      if (!(input >> station)) { return false; }
    }
  }
  return true;
}

bool ReadSelection(std::istream &input, std::vector<PlanarStationAxis> &axes, size_t faces) {
  size_t count = 0;
  if (!(input >> count) || count != axes.size()) { return false; }
  for (auto &axis : axes) {
    int64_t selected = 0;
    if (!(input >> selected) || selected < -1 || std::cmp_greater(selected, faces)) {
      return false;
    }
    if (selected == -1) { continue; }
    axis.InitialFaces.emplace(static_cast<size_t>(selected));
    for (auto &face : *axis.InitialFaces) {
      if (!(input >> face)) { return false; }
    }
  }
  return true;
}

bool WriteMesh(std::ostream &output, const PlanarMesh &mesh, const std::vector<uint64_t> &owners) {
  output.precision(std::numeric_limits<double>::max_digits10);
  output << mesh.Points.size() << ' ' << mesh.Faces.size() << '\n';
  for (const auto point : mesh.Points) { output << point[0] << ' ' << point[1] << '\n'; }
  for (size_t at = 0; at < mesh.Faces.size(); ++at) {
    const auto face = mesh.Faces[at];
    output << face[0] << ' ' << face[1] << ' ' << face[2] << ' ' << owners[mesh.FaceParents[at]]
           << '\n';
  }
  return output.good();
}

size_t CapacityBytes(const PlanarStationMesh &result) {
  const auto &mesh = result.Mesh;
  size_t bytes = mesh.Points.capacity() * sizeof(PlanarPoint) +
                 mesh.Faces.capacity() * sizeof(PlanarFace) +
                 (mesh.PointRemap.capacity() + mesh.FaceParents.capacity()) * sizeof(uint32_t) +
                 result.Stations.capacity() * sizeof(std::vector<double>);
  for (const auto &axis : result.Stations) { bytes += axis.capacity() * sizeof(double); }
  return bytes;
}
}

int main(int argc, char *const *argv) {
  if (argc != 5 && argc != 6) { return 1; }
  std::ifstream meshInput(argv[1]);
  std::ifstream axisInput(argv[2]);
  std::vector<PlanarPoint> points;
  std::vector<PlanarFace> faces;
  std::vector<uint64_t> owners;
  std::vector<PlanarStationAxis> axes;
  if (!ReadMesh(meshInput, points, faces, owners) || !ReadAxes(axisInput, axes)) { return 2; }
  if (argc == 6) {
    std::ifstream selection(argv[5]);
    if (!ReadSelection(selection, axes, faces.size())) { return 2; }
  }
  const auto start = std::chrono::steady_clock::now();
  const auto result = CutPlanarStations(points, faces, axes);
  const double elapsed =
      std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count();
  if (!result) {
    std::cerr << std::format("station mesh error={}\n", static_cast<int>(result.error()));
    return 3;
  }
  std::ofstream output(argv[3]);
  if (!WriteMesh(output, result->Mesh, owners)) { return 4; }
  output.close();
  if (!output.good()) { return 4; }
  std::ofstream stations(argv[4]);
  stations.precision(std::numeric_limits<double>::max_digits10);
  stations << result->Stations.size() << ' ' << result->Mesh.Points.size() << '\n';
  for (const auto &axis : result->Stations) {
    for (const auto value : axis) { stations << value << ' '; }
    stations << '\n';
  }
  for (const auto parent : result->Mesh.FaceParents) { stations << parent << ' '; }
  stations << '\n';
  stations.close();
  if (!stations.good()) { return 4; }
  std::cout << std::format("{{\"elapsed_us\":{},\"face_visits\":{},\"edge_intersections\":{},"
                           "\"output_capacity_bytes\":{}}}\n",
                           elapsed,
                           result->Work.FaceVisits,
                           result->Work.EdgeIntersections,
                           CapacityBytes(*result));
  return 0;
}
