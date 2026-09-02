#include "AlpineLimit.h"

#include <cmath>
#include <cstdint>

namespace outshine {

namespace {

constexpr uint32_t kJitterWordA = 0x8da6b343u;
constexpr uint32_t kJitterWordB = 0x2c1b3c6du;
constexpr uint32_t kJitterWordC = 0x297a2d39u;
constexpr uint32_t kMantissaMask = 0xFFFFFFu;
constexpr double kMantissaSteps = 16777216.0;

uint32_t Hash2(int32_t i, int32_t j) {
  uint32_t h = static_cast<uint32_t>(i) * kJitterWordA ^ static_cast<uint32_t>(j) * 0xd8163841u;
  h ^= h >> 15u;
  h *= kJitterWordB;
  h ^= h >> 12u;
  h *= kJitterWordC;
  h ^= h >> 15u;
  return h;
}

double U(uint32_t h) {
  return static_cast<double>(h & kMantissaMask) / kMantissaSteps;
}

} // namespace

bool AlpineLimit::Load(const Json::Ref &root) {
  Ready_ = false;
  Error_.clear();
  const Json::Ref a = root["alpineLimit"];
  if (a.GetKind() != Json::Kind::Object) {
    Error_ = "no alpineLimit object";
    return false;
  }
  BaseLatDeg_ = a["treelineBaseLatDeg"].Num(kTreelineBaseLatDeg);
  BaseM_ = a["treelineBaseM"].Num(kTreelineBaseM);
  PerDegM_ = a["treelinePerDegM"].Num(kTreelinePerDegM);
  BandM_ = a["treelineBandM"].Num(kTreelineBandM);
  JitterM_ = a["treelineJitterM"].Num(kTreelineJitterM);
  JitterScaleM_ = a["treelineJitterScaleM"].Num(kTreelineJitterScaleM);
  SlopeBandDeg_ = static_cast<float>(a["slopeBandDeg"].Num(4.0));
  RockTemplate_ = a["rockTemplate"].Str("");
  if (RockTemplate_.empty()) {
    Error_ = "alpineLimit.rockTemplate is empty";
    return false;
  }
  if (BandM_ <= 0.0 || JitterScaleM_ <= 0.0 || SlopeBandDeg_ <= 0.0f) {
    Error_ = "alpineLimit: band, jitter scale and slope band must be positive";
    return false;
  }
  Ready_ = true;
  return true;
}

double AlpineLimit::Noise(double e, double n) const {
  const double x = e / JitterScaleM_;
  const double y = n / JitterScaleM_;
  const double fx = std::floor(x);
  const double fy = std::floor(y);
  const auto i = static_cast<int32_t>(fx);
  const auto j = static_cast<int32_t>(fy);
  const double ux = x - fx;
  const double uy = y - fy;
  const double sx = ux * ux * (3.0 - 2.0 * ux);
  const double sy = uy * uy * (3.0 - 2.0 * uy);
  const double a = U(Hash2(i, j));
  const double b = U(Hash2(i + 1, j));
  const double c = U(Hash2(i, j + 1));
  const double d = U(Hash2(i + 1, j + 1));
  return (a + (b - a) * sx) + ((c + (d - c) * sx) - (a + (b - a) * sx)) * sy;
}

} // namespace outshine
