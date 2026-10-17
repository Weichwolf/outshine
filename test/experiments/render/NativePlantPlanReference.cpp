#include "PlantPlan.h"
#include "PlantAxisSurface.h"
#include <span>
#include <string_view>

#include <iomanip>
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

}

int main(int argc, const char *const *argv) {
  const bool surfaces = argc == 2 && std::string_view(argv[1]) == "axis-surfaces";
  if (argc > 2 || (argc == 2 && !surfaces)) { return 2; }
  using namespace outshine::Generators;
  constexpr size_t kAxes = 64;
  constexpr size_t kLeaves = 571;
  constexpr size_t kVariants = 8;
  std::vector<PlantAxis> axes(kAxes);
  std::vector<PlantLeaf> leaves(kLeaves);
  for (auto &axis : axes) {
    if (!(std::cin >> axis.Id >> axis.Parent) || !Read(axis.Path) || !Read(axis.OriginM) ||
        !Read(axis.Frame) || !(std::cin >> axis.LengthM >> axis.Bend >> axis.ParentParameter) ||
        !Read(axis.RadiiM)) {
      return 2;
    }
  }
  for (auto &leaf : leaves) {
    unsigned variant = 0;
    if (!(std::cin >> leaf.Id >> leaf.Axis >> variant) || variant >= kVariants ||
        !Read(leaf.OriginM) || !Read(leaf.NodeM) || !Read(leaf.Frame) ||
        !(std::cin >> leaf.StemParameter)) {
      return 2;
    }
    leaf.Variant = static_cast<uint8_t>(variant);
  }
  const auto plan = PlantPlan::Create(std::move(axes), std::move(leaves), kVariants);
  if (!plan) {
    std::cerr << plan.error() << '\n';
    return 3;
  }
  std::cout << std::setprecision(std::numeric_limits<double>::max_digits10);
  for (const auto &axis : plan->Axes()) {
    const auto summary = plan->Summarize(axis.Id);
    const auto detail = plan->Leaves({.Axis = axis.Id, .Maximum = kLeaves});
    if (!summary || !detail ||
        (!detail->empty() && plan->Leaves({.Axis = axis.Id, .Maximum = detail->size() - 1}))) {
      return 4;
    }
    std::cout << "S " << axis.Id << ' ' << summary->Axes << ' ' << summary->Leaves;
    Write(summary->Variants);
    std::cout << '\n';
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
}
