#include "SurfaceRibbon.h"
#include <chrono>
#include <cstddef>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <ratio>
#include <span>
#include <vector>

namespace {
bool Read(std::istream &stream, std::span<outshine::PlanarPoint> points) {
  for (auto &point : points) {
    if (!(stream >> point[0] >> point[1])) { return false; }
  }
  return true;
}
}

int main(int argc, char *const *argv) {
  if (argc != 3) { return 2; }
  using namespace outshine;
  using namespace outshine::Generators;
  std::ifstream input(argv[1]);
  size_t count = 0;
  size_t firstCount = 0;
  size_t lastCount = 0;
  SurfaceRibbonEnd first;
  SurfaceRibbonEnd last;
  if (!(input >> count >> first.StationM >> first.AxisVertex >> firstCount >> last.StationM >>
        last.AxisVertex >> lastCount) ||
      count > SurfaceRibbonBudget{}.AxisPoints ||
      firstCount > SurfaceRibbonBudget{}.ProfilePoints ||
      lastCount > SurfaceRibbonBudget{}.ProfilePoints) {
    return 2;
  }
  std::vector<PlanarPoint> axis(count);
  std::vector<PlanarPoint> firstProfile(firstCount);
  std::vector<PlanarPoint> lastProfile(lastCount);
  if (!Read(input, axis) || !Read(input, firstProfile) || !Read(input, lastProfile)) { return 2; }
  first.Profile = firstProfile;
  last.Profile = lastProfile;
  const auto began = std::chrono::steady_clock::now();
  const auto ribbon = BuildSurfaceRibbon(axis, first, last);
  const double buildMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  if (!ribbon) {
    std::cerr << "SurfaceRibbon error " << static_cast<int>(ribbon.error()) << '\n';
    return 1;
  }
  std::ofstream output(argv[2]);
  output << std::setprecision(std::numeric_limits<double>::max_digits10) << ribbon->Axis.size()
         << ' ' << ribbon->Domain.Rings[0].size() << ' ' << ribbon->AreaM2 << ' '
         << ribbon->MaximumMiter << '\n';
  for (const auto &point : ribbon->Axis) { output << point[0] << ' ' << point[1] << '\n'; }
  for (const auto &point : ribbon->Domain.Rings[0]) {
    output << point[0] << ' ' << point[1] << '\n';
  }
  std::cout << "{\"build_ms\":" << buildMs << "}\n";
  return output ? 0 : 1;
}
