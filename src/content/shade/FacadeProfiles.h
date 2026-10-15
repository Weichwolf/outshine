#ifndef OUTSHINE_CONTENT_SHADE_FACADEPROFILES_H
#define OUTSHINE_CONTENT_SHADE_FACADEPROFILES_H

#include <array>
#include <cstddef>

namespace outshine {

enum class FacadeStyle : int {
#define FACADE_PROFILE(name, value, bay, floor, width, height) name = (value),
#include "FacadeProfiles.inc"
#undef FACADE_PROFILE
};

struct FacadeProfile {
  double BayM;
  double FloorM;
  float OpeningWidth;
  float OpeningHeight;
};

inline constexpr std::array kFacadeProfiles{
#define FACADE_PROFILE(name, value, bay, floor, width, height)                                     \
  FacadeProfile{.BayM = (bay),                                                                     \
                .FloorM = (floor),                                                                 \
                .OpeningWidth = static_cast<float>(width),                                         \
                .OpeningHeight = static_cast<float>(height)},
#include "FacadeProfiles.inc"
#undef FACADE_PROFILE
};

inline constexpr int kStyleCount = static_cast<int>(kFacadeProfiles.size());

[[nodiscard]] constexpr bool ValidFacadeStyle(FacadeStyle style) {
  return static_cast<size_t>(style) < kFacadeProfiles.size();
}

[[nodiscard]] constexpr const FacadeProfile &FacadeProfileOf(FacadeStyle style) {
  return kFacadeProfiles[static_cast<size_t>(style)];
}

[[nodiscard]] constexpr std::array<float, 4> FacadeOpeningBounds(FacadeStyle style) {
  const auto &profile = FacadeProfileOf(style);
  return {(1.0f - profile.OpeningWidth) * 0.5f,
          (1.0f + profile.OpeningWidth) * 0.5f,
          (1.0f - profile.OpeningHeight) * 0.5f,
          (1.0f + profile.OpeningHeight) * 0.5f};
}
}
#endif
