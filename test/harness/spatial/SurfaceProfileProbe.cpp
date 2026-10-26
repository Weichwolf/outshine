#include "SurfaceProfile.h"
#include <cstddef>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <vector>

int main(int argc, char *const *argv) {
  if (argc != 3) { return 2; }
  using namespace outshine::Generators;
  std::ifstream input(argv[1]);
  SurfaceProfilePlane plane;
  size_t count = 0;
  if (!(input >> count >> plane.Center[0] >> plane.Center[1] >> plane.Tangent[0] >>
        plane.Tangent[1] >> plane.HalfSpanM) ||
      count > SurfaceProfileBudget{}.Triangles) {
    return 2;
  }
  std::vector<SurfaceProfileTriangle> triangles(count);
  for (auto &triangle : triangles) {
    for (auto &point : triangle) {
      if (!(input >> point[0] >> point[1] >> point[2])) { return 2; }
    }
  }
  const auto profile = CutSurfaceProfile(triangles, plane);
  if (!profile) {
    std::cerr << "SurfaceProfile error " << static_cast<int>(profile.error()) << '\n';
    return 1;
  }
  std::ofstream output(argv[2]);
  output << std::setprecision(std::numeric_limits<double>::max_digits10) << profile->Points.size()
         << ' ' << profile->AxisVertex << ' ' << profile->Work.TriangleVisits << ' '
         << profile->Work.EdgeTests << ' ' << profile->Work.ScratchHeapBytes << ' '
         << profile->Work.OutputHeapBytes << '\n';
  for (const auto &point : profile->Points) {
    output << point[0] << ' ' << point[1] << ' ' << point[2] << '\n';
  }
  for (const auto face : profile->Faces) { output << face << '\n'; }
  return output ? 0 : 1;
}
