#include "SubjectResidency.h"
#include "Check.h"

int main() {
  using namespace outshine::Render;
  using namespace outshine::Test;
  SubjectResidency held;
  const auto first = held.TakeVertices(1);
  const auto second = held.TakeVertices(1025);
  const auto indices = held.TakeIndices(1);
  held.GiveVertices(first);
  held.GiveIndices(indices);
  auto memory = held.Allocations();
  CHECK(memory.VertexSlots == 3072 && memory.FreeVertexSlots == 1024,
        "arena extent includes owned pages and reusable holes");
  CHECK(memory.IndexSlots == 4096 && memory.FreeIndexSlots == 4096,
        "a fully freed arena retains its extent without claiming live indices");
  const auto reused = held.TakeVertices(1);
  memory = held.Allocations();
  CHECK(reused.First == first.First && memory.VertexSlots == 3072 && memory.FreeVertexSlots == 0,
        "reusing a hole changes ownership without growing the arena");
  held.GiveVertices(reused);
  held.GiveVertices(second);
  memory = held.Allocations();
  CHECK(memory.FreeVertexSlots == memory.VertexSlots,
        "merged free ranges account for all released pages");
  CHECK(memory.TransferBytes == 0, "CPU range allocation creates no GPU transfer storage");
  return Report();
}
