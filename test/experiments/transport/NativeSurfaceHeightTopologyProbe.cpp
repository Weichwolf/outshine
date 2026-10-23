#include "SurfaceHeightTopology.h"
#include "SurfaceFlightPlan.h"
#include "SurfaceProbeIO.h"
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
using namespace outshine::Test::SurfaceProbes;

bool WriteFlightPlan(const char *inputPath,
                     const SurfaceHeightTopology &topology,
                     const char *outputPath) {
  std::ifstream input(inputPath);
  std::vector<SurfaceHeightRecipe> sources;
  std::vector<SurfaceFlightEndpoints> endpoints;
  if (!ReadFlights(input, topology.Sources.size(), sources, endpoints)) { return false; }
  const auto plan = BuildSurfaceFlightPlan(topology, sources, endpoints);
  return plan && WriteFlights(outputPath, *plan, topology.Points.size());
}

size_t OutputCapacityBytes(const SurfaceHeightTopology &topology) {
  size_t bytes = topology.Points.capacity() * sizeof(PlanarPoint) +
                 topology.PlanarVertices.capacity() * sizeof(uint32_t) +
                 topology.PrimaryDomains.capacity() * sizeof(uint32_t) +
                 topology.Sources.capacity() * sizeof(SurfaceHeightFaces) +
                 topology.PortVertices.capacity() * sizeof(std::vector<uint32_t>);
  for (const auto &source : topology.Sources) {
    bytes += source.Faces.capacity() * sizeof(PlanarFace) +
             source.PlanarFaces.capacity() * sizeof(uint32_t);
  }
  for (const auto &members : topology.PortVertices) {
    bytes += members.capacity() * sizeof(uint32_t);
  }
  return bytes;
}
}

int main(int argc, char *const *argv) {
  if (argc != 4 && argc != 5 && argc != 7) { return 1; }
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
  if (!WriteHeights(argv[3], *result)) { return 4; }
  if (argc >= 5 && !WriteMembers(argv[4], *result)) { return 4; }
  if (argc == 7 && !WriteFlightPlan(argv[5], *result, argv[6])) { return 5; }
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
