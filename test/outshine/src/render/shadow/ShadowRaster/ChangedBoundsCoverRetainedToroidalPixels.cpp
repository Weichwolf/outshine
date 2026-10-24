#include "Check.h"
#include "ShadowRaster.h"
#include "Box.h"
#include "Mat4.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;
constexpr uint32_t kSide = 16;

bool Covered(const ShadowRasterUpdate &update, int64_t x, int64_t y) {
  for (size_t at = 0; at < update.Count; ++at) {
    const auto &patch = update.Patches[at];
    const auto lowX = patch.Period[0] * kSide + patch.X;
    const auto lowY = patch.Period[1] * kSide + patch.Y;
    if (x >= lowX && x < lowX + patch.Width && y >= lowY && y < lowY + patch.Height) {
      return true;
    }
  }
  return false;
}

void RetainedPixels() {
  const auto initial = PrepareShadowRaster({}, {}, {}, kSide, true);
  for (int y = -3; y <= 3; ++y) {
    for (int x = -3; x <= 3; ++x) {
      Mat4 projection;
      projection[12] = 2.0 * x / kSide;
      projection[13] = -2.0 * y / kSide;
      for (int corner = 0; corner < 4; ++corner) {
        auto update = PrepareShadowRaster(initial.Next, projection, {}, kSide, false);
        const float east = (corner & 1) != 0 ? 0.75f : -0.75f;
        const float north = (corner & 2) != 0 ? 0.75f : -0.75f;
        const Boxf bounds{.Min = {{east - 0.125f, north - 0.125f, -0.25f}},
                          .Max = {{east + 0.125f, north + 0.125f, 0.25f}}};
        CHECK(IncludeShadowChange(update, bounds),
              "qualified local bounds fit fixed patch storage");
        bool complete = true;
        for (int64_t py = -y; py < kSide - y; ++py) {
          for (int64_t px = -x; px < kSide - x; ++px) {
            const double worldX = 2.0 * (static_cast<double>(px) + 0.5) / kSide - 1;
            const double worldY = 1 - 2.0 * (static_cast<double>(py) + 0.5) / kSide;
            const bool changed = worldX >= bounds.Min[0] && worldX <= bounds.Max[0] &&
                                 worldY >= bounds.Min[1] && worldY <= bounds.Max[1];
            const bool scrolled = px < 0 || px >= kSide || py < 0 || py >= kSide;
            if (changed || scrolled) { complete = complete && Covered(update, px, py); }
          }
        }
        CHECK(complete, "both changed and scrolled world pixels survive signed toroidal seams");
      }
    }
  }
}

void Fallbacks() {
  const auto initial = PrepareShadowRaster({}, {}, {}, kSide, true);
  auto update = PrepareShadowRaster(initial.Next, {}, {}, kSide, false);
  CHECK(IncludeShadowChange(update, {}) && update.Count == 0, "known empty changes keep the atlas");
  const Boxf outside{.Min = {{100, 100, 0}}, .Max = {{101, 101, 1}}};
  CHECK(IncludeShadowChange(update, outside) && update.Count == 0,
        "finite offscreen changes require no atlas writes");
  Boxf invalid{.Min = {{0, 0, 0}}, .Max = {{1, 1, 1}}};
  invalid.Min[2] = std::numeric_limits<float>::quiet_NaN();
  CHECK(!IncludeShadowChange(update, invalid), "nonfinite input requests the full fallback");
  invalid.Min[2] = 2;
  CHECK(!IncludeShadowChange(update, invalid), "reversed axes request the full fallback");
  const Boxf huge{.Min = {{-1e30f, -1e30f, -1}}, .Max = {{1e30f, 1e30f, 1}}};
  CHECK(IncludeShadowChange(update, huge) && update.Texels == uint64_t{kSide} * kSide,
        "huge finite bounds clamp before integer conversion and cover the whole tile");
  auto singular = initial.Next;
  singular.Packed[15] = 0;
  update = PrepareShadowRaster(singular, {}, {}, kSide, false);
  CHECK(!IncludeShadowChange(update, outside), "unqualified homogeneous division requests full");
}
}

int main() {
  RetainedPixels();
  Fallbacks();
  return Report();
}
