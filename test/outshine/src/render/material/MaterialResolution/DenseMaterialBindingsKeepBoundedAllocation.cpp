#include "MaterialResolution.h"
#include "Check.h"
#include "CppAllocationProbe.h"
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Render;
  using namespace outshine::Test;
  constexpr size_t count = 2048;
  std::vector<Material> materials(count);
  std::vector<ShapePart> parts(count);
  for (size_t at = 0; at < count; ++at) { parts[at].Material = static_cast<int>(count - at - 1); }
  SurfaceTable table;
  Shape shape;
  shape.Parts = parts;
  shape.Surfaces = materials;
  BeginCppAllocationProbe();
  ResolveDeclaredSurface(shape, {}, table);
  const auto allocated = EndCppAllocationProbe();
  CHECK(table.Slots.size() == count && table.PartSlot.size() == count &&
            std::cmp_equal(table.Material.front(), count - 1) && table.Material.back() == 0 &&
            table.PartSlot.front() == 0 && table.PartSlot.back() == count - 1,
        "distinct native materials retain first-use order and direct part bindings");
  const size_t outputBytes = count * (sizeof(MaterialBinding) + 2 * sizeof(int) + sizeof(uint32_t));
  const size_t scratchMost = 2 * count * sizeof(size_t);
  CHECK(
      allocated.Calls <= 6 && allocated.Bytes <= outputBytes + scratchMost,
      "dense binding allocates owned output once plus bounded index scratch, without old rasters");
  return Report();
}
