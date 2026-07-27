#include "FBFixedWeather.h"
#include <cmath>
#include <cstdio>

namespace FlightBox {

constexpr uint16_t FBFixedWeather::kIsobarHpa[FBFixedWeather::kLevels];

FBFixedWeather::FBFixedWeather(const std::string &path) {
  FILE *f = std::fopen(path.c_str(), "rb");
  if (!f) return;
  std::fseek(f, 0, SEEK_END);
  long sz = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  if (sz <= 0) { std::fclose(f); return; }
  Blob_.resize((size_t)sz);
  bool readOk = std::fread(Blob_.data(), 1, Blob_.size(), f) == Blob_.size();
  std::fclose(f);
  if (readOk) Parse();
  if (!LoadedOk_) Blob_.clear();
}

FBFixedWeather::FBFixedWeather(const uint8_t *bytes, size_t n) {
  if (!bytes || n == 0) return;
  Blob_.assign(bytes, bytes + n);   /* OWNED: the caller's buffer is a fetch result and outlives nothing */
  Parse();
  if (!LoadedOk_) Blob_.clear();
}

void FBFixedWeather::Parse() {
  const uint8_t *b = Blob_.data();
  size_t n = Blob_.size();
  if (!FBWxParseHeader(b, n, Hdr_)) return;
  Fields_.resize(Hdr_.FieldCount);
  for (int i = 0; i < (int)Hdr_.FieldCount; i++)
    if (!FBWxParseField(b, n, Hdr_, i, Fields_[(size_t)i])) return;

  SfcU_ = Find(FBWxVar::WindU, FBWxLevel::Agl, kSurfaceAglM);
  SfcV_ = Find(FBWxVar::WindV, FBWxLevel::Agl, kSurfaceAglM);
  for (int k = 0; k < kLevels; k++) {
    U_[k] = Find(FBWxVar::WindU, FBWxLevel::Isobaric, kIsobarHpa[k]);
    V_[k] = Find(FBWxVar::WindV, FBWxLevel::Isobaric, kIsobarHpa[k]);
    H_[k] = Find(FBWxVar::Height, FBWxLevel::Isobaric, kIsobarHpa[k]);
  }
  Total_ = Find(FBWxVar::Cloud, FBWxLevel::Atmosphere, 0);
  Low_   = Find(FBWxVar::Cloud, FBWxLevel::CloudLow, 0);
  Mid_   = Find(FBWxVar::Cloud, FBWxLevel::CloudMid, 0);
  High_  = Find(FBWxVar::Cloud, FBWxLevel::CloudHigh, 0);
  Ceil_  = Find(FBWxVar::Height, FBWxLevel::CloudCeil, 0);
  Vis_   = Find(FBWxVar::Vis, FBWxLevel::Surface, 0);
  LoadedOk_ = true;
}

int FBFixedWeather::Find(FBWxVar var, FBWxLevel level, uint16_t levelValue) const {
  for (size_t i = 0; i < Fields_.size(); i++) {
    const FBWxField &f = Fields_[i];
    if (f.Var == var && f.Level == level && f.LevelValue == levelValue) return (int)i;
  }
  return -1;
}

/* Bilinear, missing-aware: a corner with no value is dropped from the weighted mean instead of counting
 * as zero, which is the only reading of §9.5 that does not invent a cloud base out of four absences. */
bool FBFixedWeather::Sample(int fieldIdx, double latDeg, double lonDeg, double &out) const {
  if (!LoadedOk_ || fieldIdx < 0) return false;
  const FBWxField &f = Fields_[(size_t)fieldIdx];
  const int nx = Hdr_.Nx, ny = Hdr_.Ny;
  const double span = (double)nx * (double)Hdr_.DLon;

  double lon = lonDeg;
  if (span > 0.0) {   /* longitudes wrap, latitudes do not (§9.4) */
    lon = std::fmod(lon - (double)Hdr_.Lon0, span);
    if (lon < 0.0) lon += span;
    lon += (double)Hdr_.Lon0;
  }
  double fi = (lon - (double)Hdr_.Lon0) / (double)Hdr_.DLon;
  double fj = (latDeg - (double)Hdr_.Lat0) / (double)Hdr_.DLat;
  if (fj < 0.0) fj = 0.0;
  if (fj > (double)(ny - 1)) fj = (double)(ny - 1);

  int i0 = (int)std::floor(fi);
  double tc = fi - (double)i0;
  int j0 = (int)std::floor(fj);
  if (j0 > ny - 2) j0 = ny - 2;
  if (j0 < 0) j0 = 0;
  double tr = fj - (double)j0;
  const bool wrap = (Hdr_.Flags & kFBWxHdrFlagLonWrap) != 0;
  int i1;
  if (wrap) {
    i0 = ((i0 % nx) + nx) % nx;
    i1 = (i0 + 1) % nx;
  } else {
    if (i0 > nx - 2) i0 = nx - 2;
    if (i0 < 0) i0 = 0;
    i1 = i0 + 1;
  }

  const int ci[4] = {i0, i1, i0, i1};
  const int cj[4] = {j0, j0, j0 + 1, j0 + 1};
  const double cw[4] = {(1.0 - tc) * (1.0 - tr), tc * (1.0 - tr), (1.0 - tc) * tr, tc * tr};
  double sum = 0.0, wsum = 0.0;
  for (int k = 0; k < 4; k++) {
    double v = 0.0;
    if (!FBWxSample(Blob_.data(), Hdr_, f, ci[k], cj[k], v)) continue;
    sum += v * cw[k];
    wsum += cw[k];
  }
  if (wsum <= 0.0) return false;
  out = sum / wsum;
  return true;
}

bool FBFixedWeather::SampleField(FBWxVar var, FBWxLevel level, uint16_t levelValue, double latDeg,
                                 double lonDeg, double &out) const {
  return Sample(Find(var, level, levelValue), latDeg, lonDeg, out);
}

FBWindNed FBFixedWeather::WindNedMs(double latDeg, double lonDeg, double altM) const {
  FBWindNed w;
  if (!LoadedOk_) return w;

  /* The profile, bottom up: the 10 m field anchored at 10 m ASL, then each pressure level at ITS OWN
   * sampled geopotential height. Anchoring the surface wind to sea level rather than to the terrain is
   * the one approximation here — a terrain-aware surface layer needs the elevation hook as a second
   * input, and above ~1 km (where this simulator flies) the isobaric levels carry the answer anyway. */
  double h[kLevels + 1], u[kLevels + 1], v[kLevels + 1];
  int n = 0;
  double su = 0.0, sv = 0.0;
  if (Sample(SfcU_, latDeg, lonDeg, su) && Sample(SfcV_, latDeg, lonDeg, sv)) {
    h[n] = (double)kSurfaceAglM; u[n] = su; v[n] = sv; n++;
  }
  for (int k = 0; k < kLevels; k++) {
    double hk = 0.0, uk = 0.0, vk = 0.0;
    if (!Sample(H_[k], latDeg, lonDeg, hk)) continue;
    if (!Sample(U_[k], latDeg, lonDeg, uk) || !Sample(V_[k], latDeg, lonDeg, vk)) continue;
    if (n > 0 && hk <= h[n - 1]) continue;   /* a level below the one under it is not a profile */
    h[n] = hk; u[n] = uk; v[n] = vk; n++;
  }
  if (n == 0) return w;

  double eu, ev;
  if (altM <= h[0]) {
    eu = u[0]; ev = v[0];
  } else if (altM >= h[n - 1]) {
    eu = u[n - 1]; ev = v[n - 1];   /* nothing above 250 hPa in the blob: hold the top level */
  } else {
    int k = 0;
    while (k < n - 2 && altM > h[k + 1]) k++;
    double t = (altM - h[k]) / (h[k + 1] - h[k]);
    eu = u[k] + (u[k + 1] - u[k]) * t;
    ev = v[k] + (v[k + 1] - v[k]) * t;
  }
  /* GFS u is eastward and v northward, which IS the NED convention minus the vertical (§9.5). */
  w.N = ev;
  w.E = eu;
  w.D = 0.0;
  return w;
}

FBCloudLayers FBFixedWeather::CloudLayers(double latDeg, double lonDeg) const {
  FBCloudLayers c;
  if (!LoadedOk_) return c;
  Sample(Total_, latDeg, lonDeg, c.TotalPct);
  Sample(Low_, latDeg, lonDeg, c.LowPct);
  Sample(Mid_, latDeg, lonDeg, c.MidPct);
  Sample(High_, latDeg, lonDeg, c.HighPct);
  c.HaveCeiling = Sample(Ceil_, latDeg, lonDeg, c.CeilingM);
  return c;
}

double FBFixedWeather::VisibilityM(double latDeg, double lonDeg) const {
  double v = 0.0;
  if (!LoadedOk_ || !Sample(Vis_, latDeg, lonDeg, v)) return kFBVisibilityUnlimitedM;
  return v;
}

} // namespace FlightBox
