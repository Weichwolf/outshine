#include "SurfaceFlightPreparation.h"
#include "SurfaceProbeIO.h"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <format>
#include <fstream>
#include <iostream>
#include <ratio>
#include <vector>

int main(int argc, char *const *argv) {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test::SurfaceProbes;
  if (argc != 8) { return 1; }
  std::ifstream meshInput(argv[1]);
  std::ifstream portInput(argv[2]);
  std::ifstream flightInput(argv[3]);
  PlanarArrangement arrangement;
  std::vector<SurfaceHeightPort> ports;
  std::vector<SurfaceHeightRecipe> sources;
  std::vector<SurfaceFlightEndpoints> flights;
  uint32_t domains = 0;
  double distance = 0;
  if (!ReadMesh(meshInput, arrangement) || !ReadPorts(portInput, ports, domains, distance) ||
      !ReadFlights(flightInput, domains, sources, flights)) {
    return 2;
  }
  const auto began = std::chrono::steady_clock::now();
  const auto result =
      PrepareSurfaceFlights(arrangement, domains, ports, distance, sources, flights);
  const double elapsed =
      std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - began).count();
  if (!result) { return 3; }
  if (!WriteArrangement(argv[4], result->Arrangement) || !WriteHeights(argv[5], result->Heights) ||
      !WriteMembers(argv[6], result->Heights) ||
      !WriteFlights(argv[7], result->Plan, result->Heights.Points.size())) {
    return 4;
  }
  std::cout << std::format(
      "{{\"elapsed_us\":{},\"point_projections\":{},\"flight_vertex_visits\":{},"
      "\"gap_candidates\":{},\"station_bindings\":{},\"cut_face_visits\":{},"
      "\"edge_intersections\":{},\"initial_port_candidates\":{},\"final_port_candidates\":{}}}\n",
      elapsed,
      result->Plan.Work.PointProjections,
      result->Plan.Work.VertexVisits,
      result->Plan.Work.GapCandidates,
      result->StationBindings,
      result->StationWork.FaceVisits,
      result->StationWork.EdgeIntersections,
      result->InitialTopologyWork.PortCandidates,
      result->Heights.Work.PortCandidates);
  return 0;
}
