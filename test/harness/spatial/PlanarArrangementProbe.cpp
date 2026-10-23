#include "PlanarArrangement.h"
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

bool ReadLine(std::istream &input, PlanarLine &line) {
  size_t count = 0;
  if (!(input >> count) || count > PlanarArrangementBudget{}.InputSegments + 1) { return false; }
  line.resize(count);
  for (auto &point : line) {
    if (!(input >> point[0] >> point[1])) { return false; }
  }
  return true;
}

bool Read(std::istream &input,
          std::vector<PlanarDomain> &domains,
          std::vector<PlanarLine> &lines,
          PlanarArrangementRecipe &recipe) {
  size_t domainCount = 0;
  size_t lineCount = 0;
  if (!(input >> domainCount >> lineCount >> recipe.WeldDistanceM >>
        recipe.MaximumAreaDifferenceM2) ||
      domainCount > 64 || lineCount > 4096) {
    return false;
  }
  domains.resize(domainCount);
  lines.resize(lineCount);
  for (auto &domain : domains) {
    size_t count = 0;
    if (!(input >> count) || count > 4096) { return false; }
    domain.Rings.resize(count);
    for (auto &ring : domain.Rings) {
      if (!ReadLine(input, ring)) { return false; }
    }
  }
  for (auto &line : lines) {
    if (!ReadLine(input, line)) { return false; }
  }
  return true;
}
}

int main(int argc, char *const *argv) {
  if (argc != 3) { return 1; }
  std::ifstream input(argv[1]);
  std::vector<PlanarDomain> domains;
  std::vector<PlanarLine> lines;
  PlanarArrangementRecipe recipe;
  if (!Read(input, domains, lines, recipe)) { return 2; }
  const auto start = std::chrono::steady_clock::now();
  const auto result = ArrangePlanarDomains(domains, lines, recipe);
  const double elapsed =
      std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count();
  if (!result) {
    std::cerr << std::format("arrangement error={}\n", std::to_underlying(result.error()));
    return 3;
  }
  std::ofstream output(argv[2]);
  output.precision(std::numeric_limits<double>::max_digits10);
  output << result->Mesh.Points.size() << ' ' << result->Mesh.Faces.size() << '\n';
  for (const auto point : result->Mesh.Points) { output << point[0] << ' ' << point[1] << '\n'; }
  for (size_t at = 0; at < result->Mesh.Faces.size(); ++at) {
    const auto face = result->Mesh.Faces[at];
    output << face[0] << ' ' << face[1] << ' ' << face[2] << ' ' << result->FaceDomains[at] << '\n';
  }
  output.close();
  if (!output.good()) { return 4; }
  const auto &work = result->Work;
  std::cout << std::format("{{\"elapsed_us\":{},\"input_segments\":{},\"noded_segments\":{},"
                           "\"cells\":{},\"domain_tests\":{},\"area_difference_m2\":{},"
                           "\"weld_m\":{},\"split_edges\":{}}}\n",
                           elapsed,
                           work.InputSegments,
                           work.NodedSegments,
                           work.Cells,
                           work.DomainTests,
                           work.MaximumAreaDifferenceM2,
                           result->Mesh.Work.MaximumWeldDistance,
                           result->Mesh.Work.SplitEdges);
  return 0;
}
