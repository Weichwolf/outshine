#include "Check.h"
#include "GroundLattice.h"
#include "GroundPatchError.h"
#include "GroundPatchMesh.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

int main() {
  using namespace outshine::Render;
  using namespace outshine::Test;
  GroundInstance tile;
  tile.Corners = {-200, 110, 220, 100, -210, -130, 240, -115};
  tile.SagInv = 1.0f / 6371000;
  tile.Row = {1, 0.2f, -0.1f, 0, -0.3f, 2, 0.1f, 0, 0.1f, -0.2f, 0.8f, 0, 100000, -10000, 20000, 1};
  std::vector<float> heights(GroundLattice::kPageNodes);
  for (size_t i = 0; i < heights.size(); ++i) {
    heights[i] = static_cast<float>(std::sin(static_cast<double>(i) * 0.3) * 19 + 900);
  }
  for (size_t patch = 0; patch < 16; ++patch) {
    const auto errors =
        GroundPatchWorldErrors(tile, patch, GroundPatchHeightErrors(heights, patch));
    std::array<std::array<double, 3>, 81> world{};
    for (size_t y = 0; y < 9; ++y) {
      for (size_t x = 0; x < 9; ++x) {
        const double u = static_cast<double>(patch % 4 * 8 + x) / 32;
        const double v = static_cast<double>(patch / 4 * 8 + y) / 32;
        std::array<double, 3> local{};
        for (size_t i = 0; i < 2; ++i) {
          local[i] = (1 - v) * ((1 - u) * tile.Corners[i] + u * tile.Corners[i + 2]) +
                     v * ((1 - u) * tile.Corners[i + 4] + u * tile.Corners[i + 6]);
        }
        local[2] =
            heights[(patch / 4 * 8 + y + 1) * GroundLattice::kPageSide + patch % 4 * 8 + x + 1] -
            0.5 * (local[0] * local[0] + local[1] * local[1]) * tile.SagInv;
        for (size_t i = 0; i < 3; ++i) {
          world[y * 9 + x][i] = tile.Row[12 + i] + tile.Row[i] * local[0] +
                                tile.Row[4 + i] * local[1] + tile.Row[8 + i] * local[2];
        }
      }
    }
    for (uint32_t level = 1; level < 4; ++level) {
      const auto mesh = MakeGroundPatchMesh(1u << level);
      CHECK(mesh.has_value(), "reference refinement builds");
      if (!mesh) { continue; }
      double actual = 0;
      for (const auto &sample : mesh->Samples) {
        std::array<double, 3> delta{};
        for (size_t k = 0; k < 6; ++k) {
          for (size_t i = 0; i < 3; ++i) {
            delta[i] += world[sample.Nodes[k]][i] * sample.Weights[k];
          }
        }
        actual = std::max(actual, std::hypot(delta[0], delta[1], delta[2]));
      }
      CHECK(actual <= errors[level] + 1e-9,
            "world-space error encloses every sagged affine refinement witness");
    }
  }
  return Report();
}
