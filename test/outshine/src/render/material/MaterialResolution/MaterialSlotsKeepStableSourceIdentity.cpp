#include "MaterialResolution.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Render;
  using namespace outshine::Test;
  std::array<Material, 4> surfaces;
  for (size_t at = 0; at < surfaces.size(); ++at) {
    surfaces[at].Roughness = static_cast<float>(at) / 4;
  }
  Material fallback;
  fallback.Roughness = 0.9f;
  std::array<ShapePart, 7> parts;
  constexpr std::array ids{2, 0, 2, -1, INT_MAX, -1, -7};
  for (size_t at = 0; at < parts.size(); ++at) { parts[at].Material = ids[at]; }
  SurfaceTable table;
  Shape shape;
  shape.Parts = parts;
  shape.Surfaces = surfaces;
  ResolveDeclaredSurface(shape, fallback, table);
  constexpr std::array order{2, 0, -1, INT_MAX, -7, 1, 3};
  constexpr std::array<uint32_t, 7> slots{0, 1, 0, 2, 3, 2, 4};
  CHECK(std::ranges::equal(table.Material, order),
        "first use determines slot order before unused declarations");
  CHECK(std::ranges::equal(table.PartSlot, slots),
        "repeated valid and foreign identifiers retain their own stable slots");
  CHECK(table.Slots.size() == order.size() && table.NativeMaterial.size() == order.size(),
        "each material binding carries exactly one slot and source identity");
  for (size_t at = 0; at < order.size() && at < table.Slots.size(); ++at) {
    const int material = order[at];
    const float roughness = material >= 0 && std::cmp_less(material, surfaces.size())
                                ? surfaces[static_cast<size_t>(material)].Roughness
                                : fallback.Roughness;
    CHECK(table.Slots[at].Row.Roughness == roughness && table.NativeMaterial[at] == -1,
          "native rows and independent foreign fallbacks preserve their material values");
  }
  shape.Parts = {};
  ResolveDeclaredSurface(shape, fallback, table);
  constexpr std::array declarations{0, 1, 2, 3};
  CHECK(std::ranges::equal(table.Material, declarations) && table.PartSlot.empty(),
        "unused declarations are retained and a reused output forgets old parts");
  shape.Parts = parts;
  shape.Surfaces = {};
  ResolveDeclaredSurface(shape, fallback, table);
  CHECK(table.Slots.size() == 1 && table.Slots[0].Row.Roughness == fallback.Roughness &&
            table.Material == std::vector<int>{0} && table.NativeMaterial == std::vector<int>{-1} &&
            table.PartSlot == std::vector<uint32_t>(parts.size(), 0),
        "an absent declaration table uses the established single fallback for every part");
  ResolveDeclaredSurface({}, fallback, table);
  CHECK(table.Slots.size() == 1 && table.PartSlot.empty(),
        "an empty shape keeps its fallback without stale part bindings");
  return Report();
}
