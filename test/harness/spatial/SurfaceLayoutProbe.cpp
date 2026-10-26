#include "SurfaceLayout.h"
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <span>
#include <utility>
#include <variant>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;

struct Source {
  PlanarDomain Domain;
  PlanarLine Axis;
  double Width = 0;
  bool Steps = false;
};

bool ReadLine(std::istream &stream, PlanarLine &line, size_t &remaining) {
  size_t count = 0;
  if (!(stream >> count) || count > remaining) { return false; }
  remaining -= count;
  line.resize(count);
  for (auto &point : line) {
    if (!(stream >> point[0] >> point[1])) { return false; }
  }
  return true;
}

bool ReadSource(std::istream &stream, Source &source, size_t &remaining) {
  size_t rings = 0;
  if (!(stream >> source.Width >> source.Steps >> rings) || rings > remaining / 3 ||
      !ReadLine(stream, source.Axis, remaining)) {
    return false;
  }
  source.Domain.Rings.resize(rings);
  for (auto &ring : source.Domain.Rings) {
    if (!ReadLine(stream, ring, remaining)) { return false; }
  }
  return true;
}

void WriteLine(std::ostream &stream, std::span<const PlanarPoint> points) {
  stream << points.size() << '\n';
  for (const auto &point : points) { stream << point[0] << ' ' << point[1] << '\n'; }
}

bool Write(const char *path, const SurfaceLayout &layout) {
  std::ofstream stream(path);
  stream << std::setprecision(std::numeric_limits<double>::max_digits10) << layout.Ports.size()
         << ' ' << layout.Constraints.size() << ' ' << layout.Sections.size() << ' '
         << layout.Arrangement.Mesh.Points.size() << ' ' << layout.Arrangement.Mesh.Faces.size()
         << '\n';
  for (const auto &port : layout.Ports) {
    stream << port.Domains.size() << ' ' << port.Region.Rings.size() << '\n';
    for (const auto owner : port.Domains) { stream << owner << ' '; }
    stream << '\n';
    for (const auto &ring : port.Region.Rings) { WriteLine(stream, ring); }
  }
  for (const auto &range : layout.Sections) { stream << range.First << ' ' << range.Count << '\n'; }
  for (const auto &line : layout.Constraints) { WriteLine(stream, line); }
  for (const auto &point : layout.Arrangement.Mesh.Points) {
    stream << point[0] << ' ' << point[1] << '\n';
  }
  for (size_t at = 0; at < layout.Arrangement.Mesh.Faces.size(); ++at) {
    for (const auto vertex : layout.Arrangement.Mesh.Faces[at]) { stream << vertex << ' '; }
    stream << layout.Arrangement.FaceDomains[at] << '\n';
  }
  stream << layout.Work.InputCoordinates << ' ' << layout.Work.LandingPairs << ' '
         << layout.Work.SectionQueries << ' ' << layout.Work.SectionParts << ' '
         << layout.Work.OutputCoordinates << ' ' << layout.Work.CopiedDomainPoints << ' '
         << layout.Work.ConstraintQueries << ' ' << layout.Work.ConstraintEdgeTests << '\n';
  return static_cast<bool>(stream);
}
}

int main(int argc, char *const *argv) {
  if (argc != 3) { return 2; }
  std::ifstream stream(argv[1]);
  size_t sources = 0;
  size_t nodes = 0;
  SurfaceLayoutRecipe recipe;
  double spacing = 0;
  if (!(stream >> sources >> nodes >> recipe.LandingReachM >> spacing) ||
      sources > SurfaceLayoutBudget{}.Sources || nodes > SurfaceLayoutBudget{}.Landings) {
    return 2;
  }
  recipe.SectionSpacingM = spacing;
  size_t remaining = SurfaceLayoutBudget{}.InputCoordinates;
  std::vector<Source> storage(sources);
  std::vector<SurfaceLayoutSource> views;
  for (auto &source : storage) {
    if (!ReadSource(stream, source, remaining)) { return 2; }
    views.push_back({.Domain = source.Domain,
                     .Axis = source.Axis,
                     .WidthM = source.Width,
                     .Steps = source.Steps});
  }
  std::vector<std::vector<uint32_t>> owners(nodes);
  std::vector<SurfaceLanding> landings;
  for (auto &ids : owners) {
    PlanarPoint origin{};
    size_t count = 0;
    if (!(stream >> origin[0] >> origin[1] >> count) || count > sources) { return 2; }
    ids.resize(count);
    for (auto &id : ids) {
      if (!(stream >> id)) { return 2; }
    }
    landings.push_back({.Origin = origin, .Sources = ids});
  }
  const auto layout = BuildSurfaceLayout(views, landings, recipe);
  if (!layout) {
    std::visit(
        [](const auto error) { std::cerr << "layout error " << static_cast<int>(error) << '\n'; },
        layout.error());
    return 1;
  }
  return Write(argv[2], *layout) ? 0 : 1;
}
