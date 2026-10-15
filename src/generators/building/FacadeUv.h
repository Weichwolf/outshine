#ifndef OUTSHINE_GENERATORS_BUILDING_FACADEUV_H
#define OUTSHINE_GENERATORS_BUILDING_FACADEUV_H

#include <algorithm>
#include <cmath>
#include "FacadeProfiles.h"

namespace outshine {

enum class Facade : int {
  Wall = 0,
  RoofPitch = 1,
  RoofFlat = 2,
  Soffit = 3,
  Ledge = 4,
  Trim = 5,
  Metal = 6,
  Parapet = 7,
  Plinth = 8,
  Kerb = 9,
  Pavement = 10,
  Glass = 11
};

enum class Fields : int { Back = 0, Pane = 1, Entrance = 2 };

constexpr float kBayCeil = 256.0f;
constexpr int kFieldsCount = 3;
constexpr int kFacadeVariants = 8;

constexpr int kFacadeStride = 16;
constexpr int kIdentCount = 64;

[[nodiscard]] inline double FacadeBays(double lengthM, double bayM) {
  constexpr double kLeastWallM = 1.9;
  return lengthM < kLeastWallM
             ? 0.0
             : std::clamp(std::round(lengthM / bayM), 1.0, static_cast<double>(kBayCeil - 1.0f));
}

[[nodiscard]] constexpr bool IsGlazingUv(float u) {
  return u >= 0.0f &&
         (static_cast<int>(u / kBayCeil) % kStyleCount == static_cast<int>(FacadeStyle::Glazing) ||
          static_cast<int>(u / (kBayCeil * kStyleCount)) % kFieldsCount ==
              static_cast<int>(Fields::Pane));
}

[[nodiscard]] constexpr float FacadeUvX(FacadeStyle style, Fields standing, int ident, float bay) {
  return kBayCeil * static_cast<float>(static_cast<int>(style) +
                                       kStyleCount * (static_cast<int>(standing) +
                                                      kFieldsCount * (ident % kFacadeVariants))) +
         bay;
}

[[nodiscard]] constexpr float FacadeUvY(float storeysOverFoot) {
  return 1.0f + storeysOverFoot;
}

[[nodiscard]] constexpr float FaceUvX(Facade kind, int ident) {
  return -static_cast<float>(1 + static_cast<int>(kind) + kFacadeStride * ident);
}

}
#endif
