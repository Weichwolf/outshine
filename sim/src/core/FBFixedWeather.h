/* The weather hook's REAL-DATA implementation: one FBWX blob (tiles/src/wxfmt.h, mirrored in
 * FBWxFormat.h), bilinear in its grid, linear between the pressure levels' own geopotential heights.
 * Two constructors and one code path behind them — from FILE for the fixed gym dataset, from MEMORY for
 * the browser's one /wx fetch per session. core/ and not world/ for FBBakedDemElevation's reason: this
 * is a static data load, the same category of I/O JSBSim does for its own model XML.
 *
 * NO tick-path allocation: the blob is loaded once, the field indices are resolved once, and a sample is
 * arithmetic on the bytes. doc/flightbox/world-and-terrain.md §9. */
#ifndef FBFIXEDWEATHER_H
#define FBFIXEDWEATHER_H
#include <cstdint>
#include <string>
#include <vector>
#include "FBWeatherProvider.h"
#include "FBWxFormat.h"

namespace FlightBox {

class FBFixedWeather : public FBWeatherProvider {
public:
  /* Ok() false on any read/format failure; the provider then behaves exactly like FBCalmWeather, so a
   * missing or truncated blob degrades to still air rather than to a broken run. The CALLER decides
   * whether that is acceptable — the mission runner rejects it, the browser flies on. */
  explicit FBFixedWeather(const std::string &path);
  FBFixedWeather(const uint8_t *bytes, size_t n);

  bool Ok() const { return LoadedOk_; }
  const FBWxHeader &Header() const { return Hdr_; }
  uint32_t RunEpoch() const { return Hdr_.RunEpoch; }

  FBWindNed WindNedMs(double latDeg, double lonDeg, double altM) const override;
  FBCloudLayers CloudLayers(double latDeg, double lonDeg) const override;
  double VisibilityM(double latDeg, double lonDeg) const override;

  /* One named field, bilinear, at a point — the level below the interpolation, so a test can put a
   * documented grid-point value beside the byte that carries it. False = field absent or no value here. */
  bool SampleField(FBWxVar var, FBWxLevel level, uint16_t levelValue, double latDeg, double lonDeg,
                   double &out) const;

private:
  void Parse();
  int  Find(FBWxVar var, FBWxLevel level, uint16_t levelValue) const;
  bool Sample(int fieldIdx, double latDeg, double lonDeg, double &out) const;

  /* The four isobaric levels the blob carries, coarse to fine in altitude (§9.6). Their ORDER is the
   * profile order used by WindNedMs; the surface field sits below them. */
  static constexpr int kLevels = 4;
  static constexpr uint16_t kIsobarHpa[kLevels] = {850, 700, 500, 250};
  static constexpr uint16_t kSurfaceAglM = 10;

  bool LoadedOk_ = false;
  FBWxHeader Hdr_;
  std::vector<uint8_t> Blob_;
  std::vector<FBWxField> Fields_;
  /* Resolved once at load, so a per-tick sample never searches the descriptor table. -1 = not carried. */
  int SfcU_ = -1, SfcV_ = -1;
  int U_[kLevels] = {-1, -1, -1, -1}, V_[kLevels] = {-1, -1, -1, -1}, H_[kLevels] = {-1, -1, -1, -1};
  int Total_ = -1, Low_ = -1, Mid_ = -1, High_ = -1, Ceil_ = -1, Vis_ = -1;
};

} // namespace FlightBox
#endif
