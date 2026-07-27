/* fb-test-weather: the sim-side FBWX mirror against the WIRE, not against a transcription. It parses the
 * committed fixture (assets/wx-2026-07-27T00Z.wxb, a byte copy of one real GET /wx body) and re-derives
 * the spot values doc/flightbox/world-and-terrain.md §9.7 published from an INDEPENDENT decoder
 * (ecCodes 2.41), so a drift between tiles/src/wxfmt.h and core/FBWxFormat.h shows up as a wrong number
 * rather than as silence. Then: header self-description (a synthetic blob with a different grid step and
 * a different raster must parse purely from its own header), missing-value handling, wrap/clamp at the
 * grid edges, the vertical profile, and the wind conventions of FBConstantWindWeather.
 * Exit 0 = every check inside its stated tolerance, 1 = one failed and the line says which. */
#include "FBCalmWeather.h"
#include "FBConstantWindWeather.h"
#include "FBFixedWeather.h"
#include "FBLog.h"
#include "FBLogSinks.h"
#include "FBUnits.h"
#include "FBWxFormat.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace FlightBox;

namespace {

int Failures = 0;

void Check(bool ok, const char *what, double got, double want, double tol) {
  if (!ok) Failures++;
  FBLog::Info("wx", ok ? "CHECK_OK" : "CHECK_FAIL", {{"what", what}, {"got", got}, {"want", want},
                                                     {"tol", tol}});
}
void CheckBool(bool ok, const char *what) {
  if (!ok) Failures++;
  FBLog::Info("wx", ok ? "CHECK_OK" : "CHECK_FAIL", {{"what", what}});
}

/* §9.7's ecCodes column, at the two points the doc publishes in full. The tolerance is the field's own
 * QUANTISATION STEP (§9.6) — the only honest bound: the blob cannot carry a finer number than that. */
void CheckFixtureSpots(const FBFixedWeather &wx) {
  double v = 0.0;
  CheckBool(wx.SampleField(FBWxVar::WindU, FBWxLevel::Isobaric, 250, 46.5, 7.5, v), "u250 has a value");
  Check(std::fabs(v - 5.6443) <= 0.0055, "u250 at 46.5N/7.5E (ecCodes 5.64641 m/s)", v, 5.6443, 0.0055);

  CheckBool(wx.SampleField(FBWxVar::WindV, FBWxLevel::Isobaric, 250, 46.5, 7.5, v), "v250 has a value");
  Check(std::fabs(v - (-23.3106)) <= 0.0055, "v250 at 46.5N/7.5E (ecCodes -23.3125 m/s)", v, -23.3106,
        0.0055);

  CheckBool(wx.SampleField(FBWxVar::Height, FBWxLevel::Isobaric, 250, 46.5, 7.5, v), "gh250 has a value");
  Check(std::fabs(v - 10805.9) <= 11.8, "gh250 at 46.5N/7.5E (ecCodes 10804.5 m)", v, 10805.9, 11.8);

  CheckBool(wx.SampleField(FBWxVar::Vis, FBWxLevel::Surface, 0, 46.5, 7.5, v), "visibility has a value");
  Check(std::fabs(v - 24134.8) <= 0.374, "visibility at 46.5N/7.5E (ecCodes 24134.8 m)", v, 24134.8,
        0.374);

  CheckBool(wx.SampleField(FBWxVar::Height, FBWxLevel::CloudCeil, 0, 35.0, 139.5, v), "ceiling has a value");
  Check(std::fabs(v - 5948.4) <= 0.306, "ceiling at 35N/139.5E (ecCodes 5948.38 m)", v, 5948.4, 0.306);

  FBCloudLayers c = wx.CloudLayers(35.0, 139.5);
  Check(std::fabs(c.TotalPct - 95.7) <= 0.4, "total cover at 35N/139.5E (ecCodes 95.5 %)", c.TotalPct,
        95.7, 0.4);
  Check(std::fabs(c.LowPct - 29.8) <= 0.4, "low cover at 35N/139.5E (ecCodes 29.7 %)", c.LowPct, 29.8,
        0.4);
  CheckBool(c.HaveCeiling, "35N/139.5E declares a cloud base");

  double u10 = 0.0;
  CheckBool(wx.SampleField(FBWxVar::WindU, FBWxLevel::Agl, 10, -60.0, -60.0, u10), "10 m wind has a value");
  Check(std::fabs(u10 - 15.1751) <= 0.0055, "10 m u at 60S/60W (ecCodes 15.1738 m/s)", u10, 15.1751,
        0.0055);
}

/* The header the fixture states about itself, against §9.4/§9.5's table. */
void CheckFixtureHeader(const FBFixedWeather &wx) {
  const FBWxHeader &h = wx.Header();
  char run[24], valid[24];
  FBLog::Info("wx", "HEADER", {{"nx", (int)h.Nx}, {"ny", (int)h.Ny}, {"lat0", h.Lat0}, {"lon0", h.Lon0},
      {"dlat", h.DLat}, {"dlon", h.DLon}, {"fields", (int)h.FieldCount}, {"step", (int)h.GridStep},
      {"payloadB", (double)h.PayloadBytes}, {"run", FBWxIsoUtc(h.RunEpoch, run, sizeof run)},
      {"valid", FBWxIsoUtc(h.ValidEpoch, valid, sizeof valid)},
      {"flags", (int)h.Flags}, {"source", (int)h.Source}});
  Check(h.Nx == 720 && h.Ny == 361, "grid 720x361 (step 2 = 0.5 deg)", (double)h.Nx * h.Ny, 720.0 * 361,
        0.0);
  Check(h.FieldCount == 20, "20 fields", (double)h.FieldCount, 20.0, 0.0);
  Check(std::fabs(h.DLat + 0.5) < 1e-6, "dlat is NEGATIVE (row 0 = north pole)", h.DLat, -0.5, 1e-6);
  CheckBool((h.Flags & kFBWxHdrFlagLonWrap) != 0, "longitude wrap flag set");
  CheckBool(h.Source == kFBWxSourceGfs0p25, "source = NOAA GFS 0.25 deg");
  /* 1785110400 = 2026-07-27T00:00:00Z; f000 means run == valid (§9.3). */
  Check(h.RunEpoch == 1785110400u, "run epoch is the 2026-07-27 00Z cycle", (double)h.RunEpoch,
        1785110400.0, 0.0);
  CheckBool(h.RunEpoch == h.ValidEpoch, "analysis step: valid == run");
  Check((double)h.PayloadBytes == 8317440.0, "payload bytes (§9.5)", (double)h.PayloadBytes, 8317440.0,
        0.0);
}

/* Longitude wraps modulo nx, latitude clamps (§9.4) — the two edge rules a sampler gets wrong silently. */
void CheckEdges(const FBFixedWeather &wx) {
  double a = 0.0, b = 0.0;
  wx.SampleField(FBWxVar::WindU, FBWxLevel::Agl, 10, 0.0, 359.75, a);
  wx.SampleField(FBWxVar::WindU, FBWxLevel::Agl, 10, 0.0, -0.25, b);
  Check(std::fabs(a - b) < 1e-9, "lon 359.75 and -0.25 are the same point", a, b, 1e-9);
  wx.SampleField(FBWxVar::WindU, FBWxLevel::Agl, 10, 90.0, 12.0, a);
  wx.SampleField(FBWxVar::WindU, FBWxLevel::Agl, 10, 95.0, 12.0, b);
  Check(std::fabs(a - b) < 1e-9, "beyond the pole clamps to the pole row", a, b, 1e-9);
}

/* The wind an aircraft actually gets: at a level's own geopotential height it must BE that level. */
void CheckProfile(const FBFixedWeather &wx) {
  double gh = 0.0, u = 0.0, v = 0.0;
  wx.SampleField(FBWxVar::Height, FBWxLevel::Isobaric, 500, 46.5, 7.5, gh);
  wx.SampleField(FBWxVar::WindU, FBWxLevel::Isobaric, 500, 46.5, 7.5, u);
  wx.SampleField(FBWxVar::WindV, FBWxLevel::Isobaric, 500, 46.5, 7.5, v);
  FBWindNed w = wx.WindNedMs(46.5, 7.5, gh);
  FBLog::Info("wx", "PROFILE", {{"gh500M", gh}, {"u500", u}, {"v500", v}, {"windE", w.E}, {"windN", w.N}});
  Check(std::fabs(w.E - u) < 1e-6, "at gh500 the wind IS the 500 hPa wind (east)", w.E, u, 1e-6);
  Check(std::fabs(w.N - v) < 1e-6, "at gh500 the wind IS the 500 hPa wind (north)", w.N, v, 1e-6);

  double gh250 = 0.0, u250 = 0.0;
  wx.SampleField(FBWxVar::Height, FBWxLevel::Isobaric, 250, 46.5, 7.5, gh250);
  wx.SampleField(FBWxVar::WindU, FBWxLevel::Isobaric, 250, 46.5, 7.5, u250);
  FBWindNed top = wx.WindNedMs(46.5, 7.5, gh250 + 5000.0);
  Check(std::fabs(top.E - u250) < 1e-6, "above the top level the top level HOLDS", top.E, u250, 1e-6);

  double u10 = 0.0;
  wx.SampleField(FBWxVar::WindU, FBWxLevel::Agl, 10, 46.5, 7.5, u10);
  FBWindNed low = wx.WindNedMs(46.5, 7.5, 0.0);
  Check(std::fabs(low.E - u10) < 1e-6, "below the surface level the surface field holds", low.E, u10,
        1e-6);

  /* Strictly between two levels the answer must be strictly between the two winds — the one property
   * that proves the height interpolation runs at all. */
  double gh700 = 0.0, u700 = 0.0;
  wx.SampleField(FBWxVar::Height, FBWxLevel::Isobaric, 700, 46.5, 7.5, gh700);
  wx.SampleField(FBWxVar::WindU, FBWxLevel::Isobaric, 700, 46.5, 7.5, u700);
  FBWindNed mid = wx.WindNedMs(46.5, 7.5, 0.5 * (gh700 + gh));
  double lo = u700 < u ? u700 : u, hi = u700 < u ? u : u700;
  FBLog::Info("wx", "INTERP", {{"gh700M", gh700}, {"u700", u700}, {"gh500M", gh}, {"u500", u},
                               {"midE", mid.E}});
  CheckBool(mid.E >= lo - 1e-9 && mid.E <= hi + 1e-9, "midway wind lies between the two levels");
}

/* HEADER SELF-DESCRIPTION: a blob with a different grid step, raster, origin and field set must parse
 * from its own header alone. Built here rather than captured, because the point is that nothing about
 * 720x361 or step 2 is compiled in. */
std::vector<uint8_t> SyntheticBlob() {
  const uint16_t nx = 8, ny = 5;
  const uint16_t fields = 3;
  std::vector<uint8_t> b(kFBWxHeaderBytes + (size_t)fields * kFBWxDescBytes + (size_t)nx * ny * 4, 0);
  auto putU16 = [&](size_t o, uint16_t v) { b[o] = (uint8_t)(v & 0xff); b[o + 1] = (uint8_t)(v >> 8); };
  auto putU32 = [&](size_t o, uint32_t v) {
    for (int i = 0; i < 4; i++) b[o + i] = (uint8_t)((v >> (8 * i)) & 0xff);
  };
  auto putF32 = [&](size_t o, float v) { uint32_t bits; std::memcpy(&bits, &v, 4); putU32(o, bits); };

  putU32(0, kFBWxMagic);
  putU16(4, kFBWxFormatVersion);
  putU16(6, kFBWxHeaderBytes);
  putU16(8, nx);
  putU16(10, ny);
  putF32(12, 80.0f);     /* lat0 — deliberately NOT the pole */
  putF32(16, 10.0f);     /* lon0 — deliberately NOT the prime meridian */
  putF32(20, -40.0f);    /* dlat: 5 rows spanning 80N..-80S */
  putF32(24, 45.0f);     /* dlon: 8 columns of 45 deg = a full turn */
  putU32(28, 1234u);
  putU32(32, 1234u);
  putU16(40, fields);
  putU16(42, kFBWxDescBytes);
  putU32(44, (uint32_t)((size_t)nx * ny * 4));
  b[48] = kFBWxHdrFlagLonWrap;
  b[49] = kFBWxSourceGfs0p25;
  putU16(50, 180);       /* a grid step no production blob uses */

  size_t d0 = kFBWxHeaderBytes, d1 = d0 + kFBWxDescBytes, d2 = d1 + kFBWxDescBytes;
  size_t p0 = d2 + kFBWxDescBytes, p1 = p0 + (size_t)nx * ny, p2 = p1 + (size_t)nx * ny;
  /* Both wind components on one AGL level and nothing else: a legal blob whose profile has exactly one
   * rung, so the height logic has to fall back to it at every altitude. */
  b[d0 + 0] = (uint8_t)FBWxVar::WindU;
  b[d0 + 1] = (uint8_t)FBWxLevel::Agl;
  putU16(d0 + 2, 10);
  b[d0 + 4] = 8;
  putF32(d0 + 8, 0.5f);      /* scale */
  putF32(d0 + 12, -10.0f);   /* offset */
  putU32(d0 + 16, (uint32_t)p0);
  putU32(d0 + 20, (uint32_t)((size_t)nx * ny));

  b[d1 + 0] = (uint8_t)FBWxVar::WindV;
  b[d1 + 1] = (uint8_t)FBWxLevel::Agl;
  putU16(d1 + 2, 10);
  b[d1 + 4] = 8;
  putF32(d1 + 8, 0.5f);
  putF32(d1 + 12, -10.0f);
  putU32(d1 + 16, (uint32_t)p1);
  putU32(d1 + 20, (uint32_t)((size_t)nx * ny));

  b[d2 + 0] = (uint8_t)FBWxVar::Height;
  b[d2 + 1] = (uint8_t)FBWxLevel::CloudCeil;
  b[d2 + 4] = 16;
  b[d2 + 5] = kFBWxFldFlagMissing;
  putU16(d2 + 6, 65535);
  putF32(d2 + 8, 2.0f);
  putF32(d2 + 12, 0.0f);
  putU32(d2 + 16, (uint32_t)p2);
  putU32(d2 + 20, (uint32_t)((size_t)nx * ny * 2));

  for (size_t k = 0; k < (size_t)nx * ny; k++) b[p0 + k] = 40;   /* -10 + 40*0.5 = 10 m/s eastward */
  for (size_t k = 0; k < (size_t)nx * ny; k++) b[p1 + k] = 20;   /* -10 + 20*0.5 =  0 m/s northward */
  for (size_t k = 0; k < (size_t)nx * ny; k++) {                 /* every ceiling MISSING but one */
    b[p2 + k * 2] = 0xff;
    b[p2 + k * 2 + 1] = 0xff;
  }
  size_t one = (size_t)2 * nx + 3;   /* row 2 (lat 0), column 3 (lon 145) */
  b[p2 + one * 2] = 100;             /* 100 * 2 = 200 m */
  b[p2 + one * 2 + 1] = 0;
  return b;
}

void CheckSynthetic() {
  std::vector<uint8_t> blob = SyntheticBlob();
  FBFixedWeather wx(blob.data(), blob.size());
  CheckBool(wx.Ok(), "a blob with a different step/raster/origin parses from its own header");
  if (!wx.Ok()) return;
  Check(wx.Header().GridStep == 180, "grid step read from the header, not assumed",
        (double)wx.Header().GridStep, 180.0, 0.0);

  FBWindNed w = wx.WindNedMs(0.0, 145.0, 3000.0);
  Check(std::fabs(w.E - 10.0) < 1e-9, "surface-only blob: 10 m/s eastward at any altitude", w.E, 10.0,
        1e-9);
  Check(std::fabs(w.N) < 1e-9, "and no northward component", w.N, 0.0, 1e-9);

  FBCloudLayers hit = wx.CloudLayers(0.0, 145.0);
  CheckBool(hit.HaveCeiling, "the one non-missing ceiling sample is found");
  Check(std::fabs(hit.CeilingM - 200.0) < 1e-6, "and decodes as offset + raw*scale", hit.CeilingM, 200.0,
        1e-6);
  /* Four missing corners must stay ABSENT: an interpolated 0 would be a cloud base on the deck. */
  FBCloudLayers none = wx.CloudLayers(80.0, 10.0);
  CheckBool(!none.HaveCeiling, "an all-missing neighbourhood reports NO ceiling, not 0 m");
}

/* The two conventions the rest of the simulator depends on, checked where they are defined rather than
 * inferred from a flown mission. */
void CheckConventions() {
  FBCalmWeather calm;
  FBWindNed z = calm.WindNedMs(46.0, 7.0, 3000.0);
  double still = std::fabs(z.N) + std::fabs(z.E) + std::fabs(z.D);
  Check(still == 0.0, "calm is still air", still, 0.0, 0.0);
  double visM = calm.VisibilityM(46.0, 7.0);
  Check(visM == kFBVisibilityUnlimitedM, "calm is unlimited visibility", visM,
        kFBVisibilityUnlimitedM, 0.0);

  FBConstantWindWeather west(270.0, 20.0);
  FBWindNed w = west.WindNedMs(46.0, 7.0, 3000.0);
  double kt = 20.0 * kKtToMs;
  FBLog::Info("wx", "CONVENTION", {{"fromDeg", 270.0}, {"speedKt", 20.0}, {"n", w.N}, {"e", w.E}});
  Check(std::fabs(w.E - kt) < 1e-9, "a wind FROM 270 blows EAST", w.E, kt, 1e-9);
  Check(std::fabs(w.N) < 1e-9, "and has no northward component", w.N, 0.0, 1e-9);
  FBConstantWindWeather north(0.0, 30.0);
  FBWindNed n = north.WindNedMs(46.0, 7.0, 0.0);
  Check(std::fabs(n.N + 30.0 * kKtToMs) < 1e-9, "a wind FROM 000 blows SOUTH", n.N, -30.0 * kKtToMs,
        1e-9);
}

} // namespace

int main(int argc, char **argv) {
  static FBStdoutLogSink sink;
  FBLog::SetSink(&sink);
  FBLog::SetLevel(FBLogLevel::Debug);

  std::string path = argc > 1 ? argv[1] : "assets/wx-2026-07-27T00Z.wxb";
  FBFixedWeather wx(path);
  if (!wx.Ok()) {
    FBLog::Error("wx", "FIXTURE_UNREADABLE", {{"path", path},
        {"hint", "run from sim/, or pass the blob path as argv[1]"}});
    return 1;
  }
  FBLog::Info("wx", "FIXTURE", {{"path", path}});

  /* PROBE MODE, `fb-test-weather BLOB lat lon altM`: what the blob says at one point, which is what a
   * flown mission has to be checked against. No checks, no verdict — the reader is the caller. */
  if (argc >= 5) {
    double lat = atof(argv[2]), lon = atof(argv[3]), alt = atof(argv[4]);
    FBWindNed w = wx.WindNedMs(lat, lon, alt);
    FBCloudLayers c = wx.CloudLayers(lat, lon);
    FBLog::Info("wx", "PROBE", {{"lat", lat}, {"lon", lon}, {"altM", alt},
        {"windN", w.N}, {"windE", w.E}, {"speedMs", std::sqrt(w.N * w.N + w.E * w.E)},
        {"fromDeg", std::fmod(std::atan2(-w.E, -w.N) * 180.0 / 3.14159265358979323846 + 360.0, 360.0)},
        {"visM", wx.VisibilityM(lat, lon)}, {"totalPct", c.TotalPct}, {"lowPct", c.LowPct},
        {"midPct", c.MidPct}, {"highPct", c.HighPct},
        {"ceilingM", c.HaveCeiling ? c.CeilingM : -1.0}});
    return 0;
  }
  CheckFixtureHeader(wx);
  CheckFixtureSpots(wx);
  CheckEdges(wx);
  CheckProfile(wx);
  CheckSynthetic();
  CheckConventions();

  FBLog::Info("wx", Failures == 0 ? "RESULT_OK" : "RESULT_FAIL", {{"failures", Failures}});
  return Failures == 0 ? 0 : 1;
}
