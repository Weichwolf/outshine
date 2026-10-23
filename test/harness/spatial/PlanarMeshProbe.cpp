#include "PlanarMesh.h"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <ratio>
#include <vector>

int main(int argc, char *const *argv) {
  if (argc != 3) { return 2; }
  std::ifstream input(argv[1]);
  size_t points = 0;
  size_t faces = 0;
  double weld = 0;
  if (!(input >> points >> faces >> weld) || points > 4096 || faces > 8192) { return 2; }
  std::vector<outshine::PlanarPoint> vertices(points);
  std::vector<outshine::PlanarFace> triangles(faces);
  for (auto &point : vertices) {
    if (!(input >> point[0] >> point[1])) { return 2; }
  }
  for (auto &face : triangles) {
    if (!(input >> face[0] >> face[1] >> face[2])) { return 2; }
  }
  const auto at = std::chrono::steady_clock::now();
  const auto mesh = outshine::ConformPlanarMesh(vertices, triangles, weld);
  const double elapsed =
      std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - at).count();
  if (!mesh) {
    std::cerr << "refused " << static_cast<int>(mesh.error()) << '\n';
    return 1;
  }
  std::ofstream output(argv[2]);
  output << std::setprecision(std::numeric_limits<double>::max_digits10) << mesh->Points.size()
         << ' ' << mesh->Faces.size() << '\n';
  for (const auto point : mesh->Points) { output << point[0] << ' ' << point[1] << '\n'; }
  for (size_t index = 0; index < mesh->Faces.size(); ++index) {
    const auto face = mesh->Faces[index];
    output << face[0] << ' ' << face[1] << ' ' << face[2] << ' ' << mesh->FaceParents[index]
           << '\n';
  }
  for (const auto mapped : mesh->PointRemap) { output << mapped << ' '; }
  output << '\n';
  const auto &work = mesh->Work;
  std::cout << std::setprecision(std::numeric_limits<double>::max_digits10) << "us " << elapsed
            << " weld " << work.MaximumWeldDistance << " epsilon " << work.EdgeTolerance
            << " candidates " << work.WeldCandidates << ' ' << work.EdgeCandidates << " splits "
            << work.SplitEdges << " passes " << work.Passes << " removed " << work.RemovedFaces
            << '\n';
  output.close();
  return output.good() ? 0 : 2;
}
