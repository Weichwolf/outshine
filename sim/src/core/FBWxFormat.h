/* The SIM-SIDE MIRROR of the FBWX wire format. The authoritative contract is tiles/src/wxfmt.h (prose:
 * doc/world/weather.md §9); this file exists because core/ must not include anything from
 * tiles/. Drift between the two is caught by build/fb-test-weather, which parses the committed fixture
 * and re-derives §9's documented spot values instead of trusting either transcription.
 *
 * Everything is little-endian and read at FIXED byte offsets, never as a cast onto a C struct, so no
 * padding rule can desync this reader from the writer. */
#ifndef FBWXFORMAT_H
#define FBWXFORMAT_H
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <ctime>

namespace FlightBox {

constexpr uint32_t kFBWxMagic         = 0x58574246u;   /* 'F','B','W','X' */
constexpr uint16_t kFBWxFormatVersion = 1;
constexpr uint16_t kFBWxHeaderBytes   = 64;            /* the version-1 value; a blob states its own */
constexpr uint16_t kFBWxDescBytes     = 24;

/* The unit follows from the variable code, which is why the format carries no unit field. */
enum class FBWxVar : uint8_t {
  WindU  = 1,   /* eastward wind component, m/s */
  WindV  = 2,   /* northward wind component, m/s */
  Height = 3,   /* geopotential height above MSL, m */
  Cloud  = 4,   /* cloud cover, % */
  Vis    = 5,   /* horizontal visibility, m */
};

enum class FBWxLevel : uint8_t {
  Agl = 1, Isobaric = 2, CloudLow = 3, CloudMid = 4, CloudHigh = 5,
  Atmosphere = 6, Surface = 7, CloudCeil = 8,
};

constexpr uint8_t kFBWxHdrFlagLonWrap = 0x01;   /* column nx-1 is one dlon short of column 0 again */
constexpr uint8_t kFBWxFldFlagMissing = 0x01;   /* raw == MissingRaw means "no value here" */
constexpr uint8_t kFBWxSourceGfs0p25  = 1;

struct FBWxHeader {
  uint16_t HeaderBytes = 0, Nx = 0, Ny = 0;
  float    Lat0 = 0.0f, Lon0 = 0.0f, DLat = 0.0f, DLon = 0.0f;   /* DLat is NEGATIVE: row 0 = north */
  uint32_t RunEpoch = 0, ValidEpoch = 0;
  uint16_t FieldCount = 0, DescBytes = 0;
  uint32_t PayloadBytes = 0;
  uint8_t  Flags = 0, Source = 0;
  uint16_t GridStep = 0;
};

struct FBWxField {
  FBWxVar   Var = FBWxVar::WindU;
  FBWxLevel Level = FBWxLevel::Surface;
  uint16_t  LevelValue = 0;      /* metres for Agl, hPa for Isobaric, 0 otherwise */
  uint8_t   Bits = 0, Flags = 0;
  uint16_t  MissingRaw = 0;
  float     Scale = 0.0f, Offset = 0.0f;
  uint32_t  PayloadOff = 0, PayloadBytes = 0;
};

inline uint16_t FBWxU16(const uint8_t *p) { return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8)); }
inline uint32_t FBWxU32(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
inline float FBWxF32(const uint8_t *p) {
  uint32_t bits = FBWxU32(p);
  float v;
  std::memcpy(&v, &bits, sizeof v);
  return v;
}

/* False on anything that is not a well-formed blob of a version this build knows: magic, version, the
 * self-stated sizes, and a payload that actually fits. A rejected blob is a systems boundary, so it is
 * checked rather than assumed. */
inline bool FBWxParseHeader(const uint8_t *b, size_t n, FBWxHeader &h) {
  if (!b || n < kFBWxHeaderBytes) return false;
  if (FBWxU32(b) != kFBWxMagic || FBWxU16(b + 4) != kFBWxFormatVersion) return false;
  h.HeaderBytes  = FBWxU16(b + 6);
  h.Nx           = FBWxU16(b + 8);
  h.Ny           = FBWxU16(b + 10);
  h.Lat0         = FBWxF32(b + 12);
  h.Lon0         = FBWxF32(b + 16);
  h.DLat         = FBWxF32(b + 20);
  h.DLon         = FBWxF32(b + 24);
  h.RunEpoch     = FBWxU32(b + 28);
  h.ValidEpoch   = FBWxU32(b + 32);
  h.FieldCount   = FBWxU16(b + 40);
  h.DescBytes    = FBWxU16(b + 42);
  h.PayloadBytes = FBWxU32(b + 44);
  h.Flags        = b[48];
  h.Source       = b[49];
  h.GridStep     = FBWxU16(b + 50);
  if (h.HeaderBytes < kFBWxHeaderBytes || h.DescBytes < kFBWxDescBytes) return false;
  if (h.Nx < 2 || h.Ny < 2 || h.FieldCount == 0) return false;
  if (h.DLat == 0.0f || h.DLon == 0.0f) return false;
  return (size_t)h.HeaderBytes + (size_t)h.FieldCount * h.DescBytes <= n;
}

/* The run/valid stamps as text, because a GFS cycle logged as 1.78511e+09 is unreadable and rounded to
 * uselessness by a double-formatted log field. Buffer must hold 21 bytes. */
inline const char *FBWxIsoUtc(uint32_t epoch, char *buf, size_t n) {
  time_t t = (time_t)epoch;
  struct tm g;
#ifdef _WIN32
  gmtime_s(&g, &t);
#else
  gmtime_r(&t, &g);
#endif
  snprintf(buf, n, "%04d-%02d-%02dT%02d:%02d:%02dZ", g.tm_year + 1900, g.tm_mon + 1, g.tm_mday,
           g.tm_hour, g.tm_min, g.tm_sec);
  return buf;
}

/* Each descriptor carries its own ABSOLUTE payload offset, so a reader never accumulates sizes. */
inline bool FBWxParseField(const uint8_t *b, size_t n, const FBWxHeader &h, int index, FBWxField &f) {
  if (index < 0 || index >= (int)h.FieldCount) return false;
  const uint8_t *d = b + h.HeaderBytes + (size_t)index * h.DescBytes;
  f.Var          = (FBWxVar)d[0];
  f.Level        = (FBWxLevel)d[1];
  f.LevelValue   = FBWxU16(d + 2);
  f.Bits         = d[4];
  f.Flags        = d[5];
  f.MissingRaw   = FBWxU16(d + 6);
  f.Scale        = FBWxF32(d + 8);
  f.Offset       = FBWxF32(d + 12);
  f.PayloadOff   = FBWxU32(d + 16);
  f.PayloadBytes = FBWxU32(d + 20);
  if (f.Bits != 8 && f.Bits != 16) return false;
  size_t need = (size_t)h.Nx * h.Ny * (f.Bits / 8);
  if (f.PayloadBytes < need) return false;
  return (size_t)f.PayloadOff + need <= n;
}

/* One grid sample: value = offset + raw*scale, or "no value" for a field that declares a missing raw.
 * The caller decides what absence means — never 0, never interpolated across (§9.5). */
inline bool FBWxSample(const uint8_t *b, const FBWxHeader &h, const FBWxField &f, int i, int j,
                       double &out) {
  size_t idx = (size_t)j * h.Nx + (size_t)i;
  const uint8_t *p = b + f.PayloadOff + idx * (f.Bits / 8);
  uint16_t raw = f.Bits == 8 ? (uint16_t)*p : FBWxU16(p);
  if ((f.Flags & kFBWxFldFlagMissing) && raw == f.MissingRaw) return false;
  out = (double)f.Offset + (double)raw * (double)f.Scale;
  return true;
}

} // namespace FlightBox
#endif
