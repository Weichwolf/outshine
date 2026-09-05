#ifndef OUTSHINE_BASE_MATH_RENDERFRAME_H
#define OUTSHINE_BASE_MATH_RENDERFRAME_H

#include <array>

#include <Earth.h>

namespace outshine::RenderFrame {

[[nodiscard]] constexpr double ZOfNorth(double northM) {
  return -northM;
}

[[nodiscard]] constexpr double NorthOfZ(double zM) {
  return -zM;
}

[[nodiscard]] constexpr std::array<double, 3> Of(const EastNorthUp &enu) {
  return {enu.EastM, enu.UpM, ZOfNorth(enu.NorthM)};
}

static_assert(Of({.EastM = 1.0, .NorthM = 0.0, .UpM = 0.0}) == std::array<double, 3>{1.0, 0.0, 0.0},
              "east is the renderer's +x");
static_assert(Of({.EastM = 0.0, .NorthM = 0.0, .UpM = 1.0}) == std::array<double, 3>{0.0, 1.0, 0.0},
              "up is the renderer's +y, as Filament's");
static_assert(Of({.EastM = 0.0, .NorthM = 1.0, .UpM = 0.0}) ==
                  std::array<double, 3>{0.0, 0.0, -1.0},
              "north is the renderer's -z: x cross y is +z, so the frame stays right-handed");
constexpr double kProbeM = 3.5;
static_assert(NorthOfZ(ZOfNorth(kProbeM)) == kProbeM, "the conversion is its own inverse");

} // namespace outshine::RenderFrame
#endif
