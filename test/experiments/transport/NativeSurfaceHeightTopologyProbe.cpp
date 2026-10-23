#include "SurfaceHeightTopology.h"
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
using namespace outshine::Generators;

bool ReadMesh(std::istream &input, PlanarArrangement &mesh) {
  size_t points = 0;
  size_t faces = 0;
  if (!(input >> points >> faces) || points > SurfaceHeightTopologyBudget{}.PlanarVertices ||
      faces > SurfaceHeightTopologyBudget{}.SourceFaces) {
    return false;
  }
  mesh.Mesh.Points.resize(points);
  mesh.Mesh.Faces.resize(faces);
  mesh.FaceDomains.resize(faces);
  for (auto &point : mesh.Mesh.Points) {
    if (!(input >> point[0] >> point[1])) { return false; }
  }
  for (size_t at = 0; at < faces; ++at) {
    auto &face = mesh.Mesh.Faces[at];
    if (!(input >> face[0] >> face[1] >> face[2] >> mesh.FaceDomains[at])) { return false; }
  }
  return true;
}

bool ReadRing(std::istream &input, PolygonRing &ring, size_t &points) {
  size_t vertices = 0;
  if (!(input >> vertices) || vertices > SurfaceHeightTopologyBudget{}.PortPoints - points) {
    return false;
  }
  points += vertices;
  ring.resize(vertices);
  for (auto &point : ring) {
    if (!(input >> point[0] >> point[1])) { return false; }
  }
  return true;
}

bool ReadPorts(std::istream &input,
               std::vector<SurfaceHeightPort> &ports,
               uint32_t &domains,
               double &distance) {
  size_t count = 0;
  if (!(input >> domains >> count >> distance) || domains > std::numeric_limits<uint64_t>::digits ||
      count > SurfaceHeightTopologyBudget{}.PortPoints) {
    return false;
  }
  ports.resize(count);
  size_t points = 0;
  for (auto &port : ports) {
    size_t owners = 0;
    size_t rings = 0;
    if (!(input >> owners) || owners > domains) { return false; }
    port.Domains.resize(owners);
    for (auto &owner : port.Domains) {
      if (!(input >> owner)) { return false; }
    }
    if (!(input >> rings) || rings > SurfaceHeightTopologyBudget{}.PortPoints) { return false; }
    port.Region.Rings.resize(rings);
    for (auto &ring : port.Region.Rings) {
      if (!ReadRing(input, ring, points)) { return false; }
    }
  }
  return true;
}

size_t OutputCapacityBytes(const SurfaceHeightTopology &topology) {
  size_t bytes = topology.Points.capacity() * sizeof(PlanarPoint) +
                 topology.PlanarVertices.capacity() * sizeof(uint32_t) +
                 topology.PrimaryDomains.capacity() * sizeof(uint32_t) +
                 topology.Sources.capacity() * sizeof(SurfaceHeightFaces);
  for (const auto &source : topology.Sources) {
    bytes += source.Faces.capacity() * sizeof(PlanarFace) +
             source.PlanarFaces.capacity() * sizeof(uint32_t);
  }
  return bytes;
}
}

int main(int argc, char *const *argv) {
  if (argc != 4) { return 1; }
  std::ifstream meshInput(argv[1]);
  std::ifstream portInput(argv[2]);
  PlanarArrangement mesh;
  std::vector<SurfaceHeightPort> ports;
  uint32_t domains = 0;
  double distance = 0;
  if (!ReadMesh(meshInput, mesh) || !ReadPorts(portInput, ports, domains, distance)) { return 2; }
  const auto start = std::chrono::steady_clock::now();
  const auto result = BuildSurfaceHeightTopology(mesh, domains, ports, distance);
  const double elapsed =
      std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count();
  if (!result) {
    std::cerr << std::format("height topology error={}\n", std::to_underlying(result.error()));
    return 3;
  }
  std::ofstream output(argv[3]);
  output.precision(std::numeric_limits<double>::max_digits10);
  output << result->Points.size() << ' ' << result->Sources.size() << '\n';
  for (size_t at = 0; at < result->Points.size(); ++at) {
    output << result->Points[at][0] << ' ' << result->Points[at][1] << ' '
           << result->PrimaryDomains[at] << ' ' << result->PlanarVertices[at] << '\n';
  }
  for (const auto &part : result->Sources) {
    output << part.Faces.size() << '\n';
    for (size_t at = 0; at < part.Faces.size(); ++at) {
      const auto face = part.Faces[at];
      output << face[0] << ' ' << face[1] << ' ' << face[2] << ' ' << part.PlanarFaces[at] << '\n';
    }
  }
  output.close();
  if (!output.good()) { return 4; }
  const auto &work = result->Work;
  std::cout << std::format(
      "{{\"elapsed_us\":{},\"source_vertices\":{},\"port_candidates\":{},"
      "\"joins\":{},\"index_heap_bytes\":{},\"output_vector_capacity_bytes\":{}}}\n",
      elapsed,
      work.SourceVertices,
      work.PortCandidates,
      work.Joins,
      work.IndexHeapBytes,
      OutputCapacityBytes(*result));
  return 0;
}
