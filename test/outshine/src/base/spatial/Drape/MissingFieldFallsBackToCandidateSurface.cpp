#include <array>
#include <cstdint>
#include <optional>

#include "Check.h"
#include "spatial/Drape.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const std::array<float, 9> positions{{0, 5, 0, 1, 5, 0, 0, 5, 1}};
  const std::array<uint32_t, 3> indices{{0, 1, 2}};
  const TriangleBvh surface = TriangleBvh::Over(positions, indices);
  Drape drape{.Surface = surface,
              .Field = [](EastNorth) -> std::optional<double> { return std::nullopt; }};
  CHECK(drape.Sample({.EastM = 0.25, .NorthM = -0.25}) == 5.0,
        "a missing height field sample falls back to the candidate terrain surface");
  CHECK(drape.At({.EastM = 2.0, .NorthM = -2.0}, 7.0) == 7.0,
        "the caller fallback is used only outside both candidate height sources");
  return Report();
}
