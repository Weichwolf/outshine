#include "HeightSolid.h"
#include <array>
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

int main(int argc, char *const *argv) {
  using namespace outshine;
  if (argc != 3) { return 1; }
  std::ifstream input(argv[1]);
  size_t pointCount = 0;
  size_t faceCount = 0;
  if (!(input >> pointCount >> faceCount) || pointCount > HeightSolidBudget{}.Points ||
      faceCount > HeightSolidBudget{}.SurfaceFaces) {
    return 2;
  }
  std::vector<PlanarPoint> points(pointCount);
  std::vector<double> bottoms(pointCount);
  std::vector<PlanarFace> faces(faceCount);
  std::vector<std::array<double, 3>> tops(faceCount);
  for (size_t at = 0; at < pointCount; ++at) {
    if (!(input >> points[at][0] >> points[at][1] >> bottoms[at])) { return 2; }
  }
  for (size_t at = 0; at < faceCount; ++at) {
    if (!(input >> faces[at][0] >> faces[at][1] >> faces[at][2] >> tops[at][0] >> tops[at][1] >>
          tops[at][2])) {
      return 2;
    }
  }
  const auto began = std::chrono::steady_clock::now();
  const auto solid = BuildHeightSolid(
      {.Points = points, .Faces = faces, .TopHeights = tops, .BottomHeights = bottoms});
  const double elapsed =
      std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - began).count();
  if (!solid) {
    std::cerr << std::format("height solid error={}\n", std::to_underlying(solid.error()));
    return 3;
  }
  std::ofstream output(argv[2]);
  output.precision(std::numeric_limits<double>::max_digits10);
  output << solid->Points.size() << ' ' << solid->Faces.size() << ' ' << solid->SurfaceFaces
         << '\n';
  for (const auto &point : solid->Points) {
    output << point[0] << ' ' << point[1] << ' ' << point[2] << '\n';
  }
  for (const auto face : solid->Faces) {
    output << face[0] << ' ' << face[1] << ' ' << face[2] << '\n';
  }
  for (size_t at = 0; at < solid->WallParents.size(); ++at) {
    output << solid->WallParents[at] << ' '
           << static_cast<uint32_t>(std::to_underlying(solid->WallKinds[at])) << '\n';
  }
  output.close();
  const auto &work = solid->Work;
  std::cout << std::format("{{\"elapsed_us\":{},\"vertices\":{},\"faces\":{},\"volume_m3\":{},"
                           "\"node_queries\":{},\"indexed_heights\":{},\"height_comparisons\":{},"
                           "\"intermediate_heights\":{},\"wall_queries\":{},\"edge_visits\":{},"
                           "\"index_capacity_bytes\":{}}}\n",
                           elapsed,
                           solid->Points.size(),
                           solid->Faces.size(),
                           solid->VolumeM3,
                           work.NodeQueries,
                           work.IndexedHeights,
                           work.HeightComparisons,
                           work.IntermediateHeights,
                           work.WallQueries,
                           work.EdgeVisits,
                           work.IndexCapacityBytes);
  return output.good() ? 0 : 4;
}
