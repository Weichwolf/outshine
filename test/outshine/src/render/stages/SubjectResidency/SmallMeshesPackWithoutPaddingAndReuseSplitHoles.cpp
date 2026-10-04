#include "SubjectResidency.h"
#include "Check.h"

int main() {
  using namespace outshine::Render;
  using namespace outshine::Test;
  SubjectResidency held;
  constexpr uint32_t meshes = 1000;
  constexpr uint32_t triangleVertices = 3;
  for (uint32_t mesh = 0; mesh < meshes; ++mesh) {
    const uint32_t first = mesh * triangleVertices;
    const auto vertices = held.TakeVertices(triangleVertices);
    const auto colours = held.TakeColours(triangleVertices);
    const auto indices = held.TakeIndices(triangleVertices);
    CHECK(vertices.First == first && vertices.Count == triangleVertices,
          "small meshes occupy exactly their vertex payload without address overlap");
    CHECK(colours.First == first && colours.Count == triangleVertices,
          "each float4 colour starts on its required sixteen-byte boundary");
    CHECK(indices.First == first && indices.Count == triangleVertices,
          "rebased uint32 index runs require no per-mesh page padding");
  }
  const auto memory = held.Allocations();
  CHECK(memory.VertexSlots == meshes * triangleVertices &&
            memory.IndexSlots == meshes * triangleVertices,
        "the resident range budget equals the sum of useful triangle payloads");
  held.GiveVertices({.First = 3, .Count = 3});
  held.GiveVertices({.First = 6, .Count = 3});
  const auto split = held.TakeVertices(2);
  const auto remainder = held.TakeVertices(4);
  CHECK(split.First == 3 && split.Count == 2 && remainder.First == 5 && remainder.Count == 4,
        "a released range serves differently sized replacements without growing residency");
  held.GiveVertices(split);
  held.GiveVertices({.First = 0, .Count = 3});
  held.GiveVertices(remainder);
  const auto merged = held.TakeVertices(9);
  CHECK(merged.First == 0 && merged.Count == 9 && held.VertexRoom() == meshes * triangleVertices,
        "neighbouring releases merge across both sides without moving surviving meshes");
  CHECK(held.Allocations().FreeVertexSlots == 0,
        "reused storage is live rather than counted as a remaining hole");
  return Report();
}
