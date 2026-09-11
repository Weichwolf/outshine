#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <cmath>
#include <cstdio>
#include "Check.h"
#include "TreeLeaf.h"

int main() {
  using namespace outshine::Generators;
  using namespace outshine::Test;
  for (const auto kind : {TreeSpecies::LeafKind::Broad,
                          TreeSpecies::LeafKind::Needle,
                          TreeSpecies::LeafKind::Palmate,
                          TreeSpecies::LeafKind::Pinnate,
                          TreeSpecies::LeafKind::PalmateCompound}) {
    auto leaf = TreeSpecies::kLeafUnsaid;
    leaf.Kind = kind;
    TreeMesh mesh;
    CHECK(TreeLeaf::Build(leaf, mesh).has_value(), "supported shape builds");
    CHECK(!mesh.LeafVerts.empty() && !mesh.LeafIdx.empty(),
          "every supported form produces a surface");
    bool finite = true;
    for (const float value : mesh.LeafVerts) {
      finite = finite && std::isfinite(value);
      std::printf("VERT %a\n", static_cast<double>(value));
    }
    CHECK(finite, "leaf attributes are finite");
    bool indexed = true;
    for (const auto index : mesh.LeafIdx) {
      indexed = indexed && index < mesh.LeafVertexCount();
      std::printf("INDEX %u\n", index);
    }
    CHECK(indexed, "all triangle corners address owned vertices");
  }
  size_t profiles = 0;
  for (const auto &entry : std::filesystem::directory_iterator("src/assets/world/species")) {
    if (entry.path().extension() != ".json") { continue; }
    std::ifstream stream(entry.path());
    const std::string source{std::istreambuf_iterator<char>(stream), {}};
    TreeSpecies species;
    CHECK(species.Parse(source.data(), source.size()),
          "shipped species passes shared shape validation");
    TreeMesh mesh;
    CHECK(TreeLeaf::Build(species.LeafParams(), mesh).has_value(), "shipped leaf geometry builds");
    ++profiles;
  }
  CHECK(profiles > 0, "the shipped profile corpus is present");
  return Report();
}
