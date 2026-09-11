#include <array>
#include <limits>
#include <string>
#include "Check.h"
#include "TreeLeaf.h"

int main() {
  using namespace outshine::Generators;
  using namespace outshine::Test;
  TreeMesh mesh;
  mesh.BarkVerts = {17};
  mesh.BarkIdx = {19};
  CHECK(TreeLeaf::Build(TreeSpecies::kLeafUnsaid, mesh).has_value(), "initial leaf builds");
  const auto vertices = mesh.LeafVerts;
  const auto indices = mesh.LeafIdx;
  const auto *storage = mesh.LeafVerts.data();
  auto reject = [&](const TreeSpecies::Leaf &leaf, LeafSimplification options = {}) {
    CHECK(!TreeLeaf::Build(leaf, mesh, options), "invalid leaf is rejected");
    CHECK(mesh.LeafVerts == vertices && mesh.LeafIdx == indices && mesh.LeafVerts.data() == storage,
          "failure preserves the published leaf and its storage");
  };
  for (const int segments : {3, TreeLeaf::kMaximumSegments + 1, std::numeric_limits<int>::max()}) {
    auto leaf = TreeSpecies::kLeafUnsaid;
    leaf.Segments = segments;
    reject(leaf);
  }
  for (const int count : {-1, TreeLeaf::kMaximumLeaflets + 1, std::numeric_limits<int>::max()}) {
    auto leaf = TreeSpecies::kLeafUnsaid;
    leaf.Leaflets = count;
    reject(leaf);
  }
  for (const float number :
       {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
    for (const auto field : {&TreeSpecies::Leaf::Length,
                             &TreeSpecies::Leaf::Width,
                             &TreeSpecies::Leaf::Widest,
                             &TreeSpecies::Leaf::PalmateSpread,
                             &TreeSpecies::Leaf::Curve}) {
      auto leaf = TreeSpecies::kLeafUnsaid;
      leaf.*field = number;
      reject(leaf);
    }
    reject(TreeSpecies::kLeafUnsaid, {.MaxDeviation = number});
    reject(TreeSpecies::kLeafUnsaid, {.MaxRelativeAreaError = number});
  }
  reject(TreeSpecies::kLeafUnsaid, {.MaxDeviation = -1});
  reject(TreeSpecies::kLeafUnsaid, {.MaxRelativeAreaError = -1});
  reject(TreeSpecies::kLeafUnsaid, {.MaxRelativeAreaError = 2});
  auto leaf = TreeSpecies::kLeafUnsaid;
  leaf.Kind = static_cast<TreeSpecies::LeafKind>(255);
  reject(leaf);
  leaf.Kind = TreeSpecies::LeafKind::Palmate;
  leaf.PalmateSpread = 0;
  reject(leaf);
  leaf = TreeSpecies::kLeafUnsaid;
  leaf.Kind = TreeSpecies::LeafKind::Pinnate;
  leaf.Segments = TreeLeaf::kMaximumSegments;
  leaf.Leaflets = TreeLeaf::kMaximumLeaflets;
  reject(leaf);
  leaf = TreeSpecies::kLeafUnsaid;
  leaf.Length = std::numeric_limits<float>::max();
  leaf.Curve = std::numeric_limits<float>::max();
  reject(leaf);
  for (const int segments : {4, TreeLeaf::kMaximumSegments}) {
    leaf = TreeSpecies::kLeafUnsaid;
    leaf.Segments = segments;
    TreeMesh edge;
    CHECK(TreeLeaf::Build(leaf, edge).has_value(), "segment boundaries are accepted");
    CHECK(edge.LeafVertexCount() == static_cast<size_t>((segments + 1) * 3),
          "three vertices per station");
    CHECK(edge.LeafIdx.size() == static_cast<size_t>(segments * 12), "four triangles per strip");
  }
  for (const auto kind : {TreeSpecies::LeafKind::Pinnate, TreeSpecies::LeafKind::PalmateCompound}) {
    leaf = TreeSpecies::kLeafUnsaid;
    leaf.Kind = kind;
    leaf.Segments = 4;
    leaf.Leaflets = TreeLeaf::kMaximumLeaflets;
    TreeMesh edge;
    CHECK(TreeLeaf::Build(leaf, edge).has_value(),
          "maximum leaflets fit at the minimum resolution");
  }
  CHECK(mesh.BarkVerts == std::vector<float>{17} && mesh.BarkIdx == std::vector<uint32_t>{19},
        "leaf replacement never alters bark");
  TreeSpecies species;
  const std::string initial = R"({"name":"kept"})";
  CHECK(species.Parse(initial.data(), initial.size()), "initial species parses");
  for (const std::string &field : {R"("leaf_kind":"unknown")",
                                   R"("leaf_kind":null)",
                                   R"("leaf_segments":2147483647)",
                                   R"("leaf_segments":1.5)",
                                   R"("leaf_segments":"28")",
                                   R"("leaf_width":1e100)",
                                   R"("leaf_width":null)",
                                   R"("leaf_leaflets":17)",
                                   R"("leaf_kind":"palmate","leaf_palmate_spread":0)"}) {
    const std::string source = "{\"name\":\"rejected\"," + field + "}";
    CHECK(!species.Parse(source.data(), source.size()), "invalid species shape is rejected");
    CHECK(species.Name() == "kept", "failed parsing preserves the previous species");
  }
  return Report();
}
