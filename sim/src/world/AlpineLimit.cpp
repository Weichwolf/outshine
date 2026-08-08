#include "AlpineLimit.h"

#include <cmath>
#include <cstdint>

namespace outshine::World {

namespace {

uint32_t Hash2(int32_t i, int32_t j) {
  uint32_t h = (uint32_t)i * 0x8da6b343u ^ (uint32_t)j * 0xd8163841u;
  h ^= h >> 15; h *= 0x2c1b3c6du; h ^= h >> 12; h *= 0x297a2d39u; h ^= h >> 15;
  return h;
}
double U(uint32_t h) { return (double)(h & 0xFFFFFFu) / 16777216.0; }

}  // namespace

bool AlpineLimit::Load(const Render::Json::Ref &root) {
  Ready_ = false;
  Error_.clear();
  const Render::Json::Ref a = root["alpineLimit"];
  if (a.GetKind() != Render::Json::Kind::Object) { Error_ = "no alpineLimit object"; return false; }
  BaseLatDeg_ = a["treelineBaseLatDeg"].Num(47.4);
  BaseM_ = a["treelineBaseM"].Num(1900.0);
  PerDegM_ = a["treelinePerDegM"].Num(-58.8);
  BandM_ = a["treelineBandM"].Num(200.0);
  JitterM_ = a["treelineJitterM"].Num(150.0);
  JitterScaleM_ = a["treelineJitterScaleM"].Num(700.0);
  SlopeBandDeg_ = (float)a["slopeBandDeg"].Num(4.0);
  RockTemplate_ = a["bareRockTemplate"].Str("");
  if (RockTemplate_.empty()) { Error_ = "alpineLimit.bareRockTemplate is empty"; return false; }
  if (BandM_ <= 0.0 || JitterScaleM_ <= 0.0 || SlopeBandDeg_ <= 0.0f) {
    Error_ = "alpineLimit: band, jitter scale and slope band must be positive";
    return false;
  }
  Ready_ = true;
  return true;
}

double AlpineLimit::Noise(double e, double n) const {
  const double x = e / JitterScaleM_, y = n / JitterScaleM_;
  const double fx = std::floor(x), fy = std::floor(y);
  const int32_t i = (int32_t)fx, j = (int32_t)fy;
  const double ux = x - fx, uy = y - fy;
  const double sx = ux * ux * (3.0 - 2.0 * ux), sy = uy * uy * (3.0 - 2.0 * uy);
  const double a = U(Hash2(i, j)), b = U(Hash2(i + 1, j));
  const double c = U(Hash2(i, j + 1)), d = U(Hash2(i + 1, j + 1));
  return (a + (b - a) * sx) + ((c + (d - c) * sx) - (a + (b - a) * sx)) * sy;
}

} // namespace outshine::World
