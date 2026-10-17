#include "PlantPlan.h"
#include "PlantBladeSurface.h"
#include "PlantAxisSurface.h"
#include <span>
#include <string_view>

#include <iomanip>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <iostream>
#include <utility>
#include <vector>

namespace {
template <class T> bool Read(T &values) {
  for (auto &value : values) {
    if (!(std::cin >> value)) { return false; }
  }
  return true;
}

template <class T> void Write(const T &values) {
  for (const auto value : values) { std::cout << ' ' << value; }
}

constexpr size_t kVariants = 8;

void WriteSummary(const outshine::Generators::PlantPlan &plan,
                  uint64_t id,
                  const outshine::Generators::PlantPlan::Summary &summary) {
  std::cout << "S " << id << ' ' << summary.Axes << ' ' << summary.Leaves;
  Write(summary.Variants);
  std::cout << "\nD " << id;
  for (const size_t row : summary.AxisRows) { std::cout << ' ' << plan.Axes()[row].Id; }
  std::cout << '\n';
}

bool ReadAxis(outshine::Generators::PlantAxis &axis) {
  return (std::cin >> axis.Id >> axis.Parent) && Read(axis.Path) && Read(axis.OriginM) &&
         Read(axis.Frame) && (std::cin >> axis.LengthM >> axis.Bend >> axis.ParentParameter) &&
         Read(axis.RadiiM);
}

bool ReadLeaf(outshine::Generators::PlantLeaf &leaf) {
  unsigned variant = 0;
  if (!(std::cin >> leaf.Id >> leaf.Axis >> variant) || variant >= kVariants ||
      !Read(leaf.OriginM) || !Read(leaf.NodeM) || !Read(leaf.Frame) ||
      !(std::cin >> leaf.StemParameter)) {
    return false;
  }
  leaf.Variant = static_cast<uint8_t>(variant);
  return true;
}

bool ReadBlade(outshine::Generators::PlantBlade &blade) {
  return static_cast<bool>(std::cin >> blade.LengthM >> blade.WidthM >> blade.Widest >>
                           blade.Lobes >> blade.LobeDepth >> blade.BaseFill >> blade.BendRad >>
                           blade.Fold >> blade.FoldPower >> blade.TwistRad >> blade.PetioleM);
}

bool WriteSurface(const outshine::Generators::PlantPlan &plan, uint64_t axis) {
  constexpr size_t kSegments = 24;
  constexpr size_t kSides = 8;
  constexpr size_t kVertices = 218;
  constexpr size_t kTriangles = 400;
  const auto mesh = outshine::Generators::BuildPlantAxisSurface(plan,
                                                                {.Axis = axis,
                                                                 .Segments = kSegments,
                                                                 .Sides = kSides,
                                                                 .MaximumVertices = kVertices,
                                                                 .MaximumTriangles = kTriangles});
  if (!mesh) {
    std::cerr << mesh.error() << '\n';
    return false;
  }
  for (size_t triangle = 0; triangle < mesh->Triangles.size(); triangle += 3) {
    std::cout << "G " << axis;
    for (size_t corner = 0; corner < 3; ++corner) {
      const size_t at = static_cast<size_t>(mesh->Triangles[triangle + corner]) * 3;
      Write(std::span(mesh->Positions).subspan(at, 3));
      Write(std::span(mesh->Normals).subspan(at, 3));
    }
    std::cout << '\n';
  }
  return true;
}

bool WriteBlade(const outshine::Generators::PlantBlade &blade, size_t variant) {
  constexpr size_t kSegments = 32;
  constexpr size_t kAcross = 2;
  constexpr size_t kVertices = 206;
  constexpr size_t kTriangles = 260;
  const auto mesh = outshine::Generators::BuildPlantBladeSurface(blade,
                                                                 {.Segments = kSegments,
                                                                  .Across = kAcross,
                                                                  .MaximumVertices = kVertices,
                                                                  .MaximumTriangles = kTriangles});
  if (!mesh) {
    std::cerr << mesh.error() << '\n';
    return false;
  }
  std::cout << "C " << variant << ' ' << mesh->Positions.size() / 3 << ' '
            << mesh->Triangles.size() / 3 << '\n';
  for (size_t triangle = 0; triangle < mesh->Triangles.size(); triangle += 3) {
    std::cout << "B " << variant;
    for (size_t corner = 0; corner < 3; ++corner) {
      const size_t index = mesh->Triangles[triangle + corner];
      Write(std::span(mesh->Positions).subspan(index * 3, 3));
      Write(std::span(mesh->Normals).subspan(index * 3, 3));
      Write(std::span(mesh->Texture).subspan(index * 2, 2));
    }
    std::cout << '\n';
  }
  return true;
}

}

int main(int argc, const char *const *argv) {
  const bool blades = argc == 2 && std::string_view(argv[1]) == "plant-surfaces";
  const bool surfaces = blades || (argc == 2 && std::string_view(argv[1]) == "axis-surfaces");
  if (argc > 2 || (argc == 2 && !surfaces)) { return 2; }
  using namespace outshine::Generators;
  constexpr size_t kAxes = 64;
  constexpr size_t kLeaves = 571;
  std::vector<PlantAxis> axes(kAxes);
  std::vector<PlantLeaf> leaves(kLeaves);
  if (!std::ranges::all_of(axes, ReadAxis) || !std::ranges::all_of(leaves, ReadLeaf)) { return 2; }
  const auto plan = PlantPlan::Create(std::move(axes), std::move(leaves), kVariants);
  if (!plan) {
    std::cerr << plan.error() << '\n';
    return 3;
  }
  std::vector<PlantBlade> prototypes(blades ? kVariants : 0);
  if (!std::ranges::all_of(prototypes, ReadBlade)) { return 2; }
  std::cout << std::setprecision(std::numeric_limits<double>::max_digits10);
  for (const auto &axis : plan->Axes()) {
    const auto summary = plan->Summarize(axis.Id);
    const auto detail = plan->Leaves({.Axis = axis.Id, .Maximum = kLeaves});
    if (!summary || !detail ||
        (!detail->empty() && plan->Leaves({.Axis = axis.Id, .Maximum = detail->size() - 1}))) {
      return 4;
    }
    WriteSummary(*plan, axis.Id, *summary);
    std::cout << "A " << axis.Id << ' ' << axis.Parent;
    Write(axis.Path);
    Write(axis.OriginM);
    Write(axis.Frame);
    std::cout << ' ' << axis.LengthM << ' ' << axis.Bend << ' ' << axis.ParentParameter;
    Write(axis.RadiiM);
    std::cout << '\n';
    for (const auto &leaf : *detail) {
      std::cout << "L " << leaf.Id << ' ' << leaf.Axis << ' '
                << static_cast<unsigned>(leaf.Variant);
      Write(leaf.OriginM);
      Write(leaf.NodeM);
      Write(leaf.Frame);
      std::cout << ' ' << leaf.StemParameter << '\n';
    }
    if (surfaces && !WriteSurface(*plan, axis.Id)) { return 5; }
  }
  for (size_t variant = 0; variant < prototypes.size(); ++variant) {
    if (!WriteBlade(prototypes[variant], variant)) { return 6; }
  }
}
