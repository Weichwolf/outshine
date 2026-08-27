#ifndef OUTSHINE_GENERATORS_BASE_FACADEUV_H
#define OUTSHINE_GENERATORS_BASE_FACADEUV_H

namespace outshine {

enum class Facade : int {
  Wall = 0, RoofPitch = 1, RoofFlat = 2, Soffit = 3, Ledge = 4, Trim = 5, Metal = 6, Parapet = 7,
  Plinth = 8, Kerb = 9, Pavement = 10
};

enum class FacadeStyle : int {
  Outbuilding = 0, House = 1, Terrace = 2, Block = 3, Hall = 4, Tower = 5, Spire = 6
};

enum class Standing : int { Back = 0, Front = 1, Entrance = 2 };

constexpr float kBayCeil = 256.0f;
constexpr int kStyleCount = 8;

constexpr float kStoreyCeil = 64.0f;
constexpr int kFacadeStride = 16;
constexpr int kIdentCount = 64;

inline float FacadeUvX(FacadeStyle style, Standing standing, float bay) {
  return kBayCeil * (float)((int)style + kStyleCount * (int)standing) + bay;
}

inline float FacadeUvY(int ident, float storeysOverFoot) {
  return kStoreyCeil * (float)ident + 1.0f + storeysOverFoot;
}

inline float FaceUvX(Facade kind, int ident) {
  return -(float)((int)kind + kFacadeStride * ident);
}

}
#endif
