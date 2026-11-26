#include "Species.h"
#include "TreePrototype.h"
#include "WriteFileAtomically.h"
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <system_error>
#include <utility>
#include <vector>
#include <nlohmann/json.hpp>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using Json = nlohmann::json;

bool Write(const std::filesystem::path &path, std::span<const std::byte> data) {
  return WriteFileAtomically(path.string(), data).has_value();
}

std::optional<Json> WriteMesh(const std::filesystem::path &path, const Geometry &geometry) {
  if (geometry.parts() != 1) { return std::nullopt; }
  const auto positions = geometry.positionsOf(0);
  const auto normals = geometry.normalsOf(0);
  const auto indices = geometry.trianglesOf(0);
  if (!Write(path / "positions.f32", std::as_bytes(positions)) ||
      !Write(path / "normals.f32", std::as_bytes(normals)) ||
      !Write(path / "indices.u32", std::as_bytes(indices))) {
    return std::nullopt;
  }
  const auto &material = geometry.surfaceAt(geometry.materialOf(0));
  return Json{{"vertices", positions.size() / 3},
              {"triangles", indices.size() / 3},
              {"colour", material.BaseColour.Axis}};
}

std::optional<Json>
WriteRank(const std::filesystem::path &path, const TreePrototype &tree, size_t rank) {
  auto geometry = tree.InstancedGeometryAt(rank);
  if (!geometry) { return std::nullopt; }
  std::error_code error;
  std::filesystem::create_directories(path / "bark", error);
  if (error) { return std::nullopt; }
  std::filesystem::create_directories(path / "twig", error);
  if (error) { return std::nullopt; }
  auto bark = WriteMesh(path / "bark", geometry->Bark);
  auto twig = WriteMesh(path / "twig", geometry->Leaf);
  if (!bark || !twig) { return std::nullopt; }
  std::vector<float> placements;
  constexpr size_t kFloatsPerTransform = 16;
  placements.reserve(kFloatsPerTransform * geometry->Placements.size());
  for (const auto &matrix : geometry->Placements) {
    for (const double entry : matrix.Column) { placements.push_back(static_cast<float>(entry)); }
  }
  if (!Write(path / "placements.f32", std::as_bytes(std::span(placements)))) {
    return std::nullopt;
  }
  (*twig)["placements"] = geometry->Placements.size();
  return Json{{"bark", std::move(*bark)},
              {"twig", std::move(*twig)},
              {"least_m", geometry->LeastM.Axis},
              {"most_m", geometry->MostM.Axis}};
}
}

int main(int argc, const char *const *argv) {
  if (argc != 3) {
    std::cerr << "tree_prototype <species json> <output dir>\n";
    return 2;
  }
  TreeSpecies species;
  if (!ReadSpecies(argv[1], &species)) { return 1; }
  const auto tree = TreePrototype::Grow(species);
  if (!tree) { return 1; }
  const std::filesystem::path root(argv[2]);
  for (size_t rank = 0; rank < tree->Ranks().size(); ++rank) {
    const auto path = root / std::to_string(rank);
    const auto manifest = WriteRank(path, *tree, rank);
    if (!manifest) { return 1; }
    const auto text = manifest->dump() + '\n';
    if (!Write(path / "prototype.json", std::as_bytes(std::span(text)))) { return 1; }
  }
  return 0;
}
