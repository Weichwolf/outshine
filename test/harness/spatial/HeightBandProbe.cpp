#include "HeightBandMesh.h"
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
  size_t levelCount = 0;
  const HeightBandBudget budget;
  if (!(input >> pointCount >> faceCount >> levelCount) || pointCount > budget.InputPoints ||
      faceCount > budget.InputFaces || levelCount > budget.Levels) {
    return 2;
  }
  std::vector<std::array<double, 3>> points(pointCount);
  std::vector<PlanarFace> faces(faceCount);
  std::vector<HeightLevelRange> ranges(faceCount);
  std::vector<double> levels(levelCount);
  for (auto &point : points) {
    if (!(input >> point[0] >> point[1] >> point[2])) { return 2; }
  }
  for (auto &level : levels) {
    if (!(input >> level)) { return 2; }
  }
  for (size_t at = 0; at < faceCount; ++at) {
    if (!(input >> faces[at][0] >> faces[at][1] >> faces[at][2] >> ranges[at].First >>
          ranges[at].Count)) {
      return 2;
    }
  }
  const auto began = std::chrono::steady_clock::now();
  const auto result =
      CutHeightBands({.Points = points, .Faces = faces, .LevelRanges = ranges, .Levels = levels});
  const double elapsed =
      std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - began).count();
  if (!result) {
    std::cerr << std::format("height band error={}\n", std::to_underlying(result.error()));
    return 3;
  }
  std::ofstream output(argv[2]);
  output.precision(std::numeric_limits<double>::max_digits10);
  output << result->Points.size() << ' ' << result->Faces.size() << '\n';
  for (const auto &point : result->Points) {
    output << point[0] << ' ' << point[1] << ' ' << point[2] << '\n';
  }
  for (size_t at = 0; at < result->Faces.size(); ++at) {
    const auto face = result->Faces[at];
    output << face[0] << ' ' << face[1] << ' ' << face[2] << ' ' << result->FaceParents[at] << ' '
           << result->LevelIndices[at] << '\n';
  }
  const auto &work = result->Work;
  std::cout << std::format("{{\"elapsed_us\":{},\"level_checks\":{},\"band_visits\":{},"
                           "\"skipped_bands\":{},\"search_comparisons\":{},\"edge_visits\":{},"
                           "\"intersection_queries\":{},\"new_intersections\":{},"
                           "\"removed_zero_faces\":{},\"copied_input_bytes\":{}}}\n",
                           elapsed,
                           work.LevelChecks,
                           work.BandVisits,
                           work.SkippedBands,
                           work.SearchComparisons,
                           work.EdgeVisits,
                           work.IntersectionQueries,
                           work.NewIntersections,
                           work.RemovedZeroFaces,
                           work.CopiedInputBytes);
  output.close();
  return output.good() ? 0 : 4;
}
