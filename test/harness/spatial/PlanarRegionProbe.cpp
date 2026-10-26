#include "PlanarRegionRemesh.h"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <format>
#include <fstream>
#include <iostream>
#include <ratio>
#include <utility>
#include <vector>

int main(int argc, char *const *argv) {
  using namespace outshine;
  if (argc != 3) { return 1; }
  std::ifstream input(argv[1]);
  size_t pointCount = 0;
  size_t faceCount = 0;
  size_t regionCount = 0;
  const PlanarRegionBudget budget;
  if (!(input >> pointCount >> faceCount >> regionCount) || pointCount > budget.Points ||
      faceCount > budget.Faces || regionCount > budget.Regions) {
    return 2;
  }
  std::vector<PlanarPoint> points(pointCount);
  std::vector<PlanarFace> faces(faceCount);
  std::vector<PlanarFaceRegion> regions(regionCount);
  for (auto &point : points) {
    if (!(input >> point[0] >> point[1])) { return 2; }
  }
  for (auto &face : faces) {
    if (!(input >> face[0] >> face[1] >> face[2])) { return 2; }
  }
  size_t selected = 0;
  for (auto &region : regions) {
    size_t count = 0;
    if (!(input >> count) || count > faceCount - selected) { return 2; }
    selected += count;
    region.resize(count);
    for (auto &index : region) {
      if (!(input >> index)) { return 2; }
    }
  }
  const auto began = std::chrono::steady_clock::now();
  const auto result = RemeshPlanarRegions(points, faces, regions);
  const double elapsed =
      std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - began).count();
  if (!result) {
    std::cerr << std::format("region remesh error={}\n", std::to_underlying(result.error()));
    return 3;
  }
  std::ofstream output(argv[2]);
  output << result->Regions.size() << '\n';
  for (const auto &region : result->Regions) {
    output << region.size() << '\n';
    for (const auto face : region) {
      output << face[0] << ' ' << face[1] << ' ' << face[2] << '\n';
    }
  }
  const auto &work = result->Work;
  std::cout << std::format("{{\"elapsed_us\":{},\"input_faces\":{},\"output_faces\":{},"
                           "\"ring_visits\":{},\"removed_vertices\":{},"
                           "\"maximum_chain_deviation_m\":{},\"coverage_error_m2\":{}}}\n",
                           elapsed,
                           work.InputFaces,
                           work.OutputFaces,
                           work.RingVisits,
                           work.RemovedVertices,
                           work.MaximumChainDeviationM,
                           work.CoverageErrorM2);
  output.close();
  return output.good() ? 0 : 4;
}
