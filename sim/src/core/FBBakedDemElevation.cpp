/* Asset layout (little-endian, all fields read at fixed byte offsets — never a cast onto a C struct,
 * so padding/alignment can never desync the reader from tools/bake_swiss_dem.py's writer):
 *   offset  0  char[8]   magic   "FBDEM01\0"
 *   offset  8  uint32_t  cols
 *   offset 12  uint32_t  rows
 *   offset 16  double    lonMin
 *   offset 24  double    latMin
 *   offset 32  double    lonMax
 *   offset 40  double    latMax
 *   offset 48  float     scaleM        (int16 sample * scaleM = metres)
 *   offset 52  uint32_t  reserved (0)
 *   offset 56  int16_t[rows*cols]      row-major, row 0 = latMin, col 0 = lonMin, little-endian
 */
#include "FBBakedDemElevation.h"
#include <cstdio>
#include <cstring>

namespace FlightBox {

namespace {
constexpr size_t kHeaderBytes = 56;
constexpr char kMagic[8] = {'F', 'B', 'D', 'E', 'M', '0', '1', '\0'};

uint32_t ReadU32LE(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
double ReadF64LE(const uint8_t *p) {
  uint64_t bits = 0;
  for (int i = 0; i < 8; i++) bits |= (uint64_t)p[i] << (8 * i);
  double v;
  std::memcpy(&v, &bits, sizeof v);
  return v;
}
float ReadF32LE(const uint8_t *p) {
  uint32_t bits = ReadU32LE(p);
  float v;
  std::memcpy(&v, &bits, sizeof v);
  return v;
}
int16_t ReadI16LE(const uint8_t *p) { return (int16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8)); }
} // namespace

FBBakedDemElevation::FBBakedDemElevation(const std::string &path) {
  FILE *f = std::fopen(path.c_str(), "rb");
  if (!f) return;
  std::fseek(f, 0, SEEK_END);
  long sz = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  if (sz < (long)kHeaderBytes) { std::fclose(f); return; }
  std::vector<uint8_t> buf((size_t)sz);
  bool readOk = std::fread(buf.data(), 1, buf.size(), f) == buf.size();
  std::fclose(f);
  if (!readOk || std::memcmp(buf.data(), kMagic, sizeof kMagic) != 0) return;

  Cols = ReadU32LE(&buf[8]);
  Rows = ReadU32LE(&buf[12]);
  LonMin = ReadF64LE(&buf[16]);
  LatMin = ReadF64LE(&buf[24]);
  LonMax = ReadF64LE(&buf[32]);
  LatMax = ReadF64LE(&buf[40]);
  ScaleM = ReadF32LE(&buf[48]);
  size_t need = kHeaderBytes + (size_t)Cols * (size_t)Rows * 2;
  if (Cols < 2 || Rows < 2 || buf.size() < need) { Cols = Rows = 0; return; }

  Grid.resize((size_t)Cols * (size_t)Rows);
  const uint8_t *g = &buf[kHeaderBytes];
  for (size_t i = 0; i < Grid.size(); i++) Grid[i] = ReadI16LE(g + i * 2);
  LoadedOk = true;
}

double FBBakedDemElevation::GroundElevM(double latDeg, double lonDeg) const {
  if (!LoadedOk || lonDeg < LonMin || lonDeg > LonMax || latDeg < LatMin || latDeg > LatMax) return 0.0;
  double cf = (lonDeg - LonMin) / (LonMax - LonMin) * (double)(Cols - 1);
  double rf = (latDeg - LatMin) / (LatMax - LatMin) * (double)(Rows - 1);
  int c0 = (int)cf, r0 = (int)rf;
  if (c0 > (int)Cols - 2) c0 = (int)Cols - 2;
  if (r0 > (int)Rows - 2) r0 = (int)Rows - 2;
  if (c0 < 0) c0 = 0;
  if (r0 < 0) r0 = 0;
  double tc = cf - c0, tr = rf - r0;
  auto at = [&](int c, int r) { return (double)Grid[(size_t)r * Cols + (size_t)c] * (double)ScaleM; };
  double h00 = at(c0, r0), h01 = at(c0 + 1, r0), h10 = at(c0, r0 + 1), h11 = at(c0 + 1, r0 + 1);
  double top = h00 + (h01 - h00) * tc, bot = h10 + (h11 - h10) * tc;
  return top + (bot - top) * tr;
}

} // namespace FlightBox
