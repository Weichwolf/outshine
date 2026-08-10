#include "TreeField.h"

#include "ClassStructure.h"
#include "VegetationTemplates.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace outshine::World {

namespace {

/* One cell, one draw — which makes the scatter a function of the cell and not of an order. The edge
 * must be WELL below the mean stand spacing: at 11.09 m2 the densest declared class (conifer forest,
 * 0.033/m2) accepts 36.6 % of cells, two in three stay empty, and that is exactly what stops the grid
 * from reading as rows. With an edge around the stand spacing the acceptance probability would be 1
 * and the forest a raster. */
constexpr double kCellM = 3.33;

inline uint32_t Hash(int32_t i, int32_t j, uint32_t salt) {
  uint32_t h = (uint32_t)i * 0x8da6b343u ^ (uint32_t)j * 0xd8163841u ^ salt * 0xcb1ab31fu;
  h ^= h >> 15; h *= 0x2c1b3c6du; h ^= h >> 12; h *= 0x297a2d39u; h ^= h >> 15;
  return h;
}
inline float U(uint32_t h) { return (float)(h & 0xFFFFFFu) / 16777216.0f; }
inline float U16(uint32_t h) { return (float)(h & 0xFFFFu) / 65536.0f; }

/* Triangular rather than normal, because the distribution ends HARD at mu +- 2.449 sigma: over 166 000
 * stands a normal draw will certainly produce a 4-sigma tree, and in the picture that is a defect, not
 * spread. 2.4494897 = 1/sqrt(2/12), the standard deviation of the sum of two uniforms. */
inline float SizeFactor(uint32_t h, float sigma) {
  return 1.0f + sigma * (U16(h) + U16(h >> 16) - 1.0f) * 2.4494897f;
}

}  // namespace

void TreeField::Scatter(const ClassStructure &cls, const VegetationTemplates &veg, double radiusM,
                        double eyeE, double eyeN, double eyeAsl, double camLatDeg, float sizeSigma,
                        const Crown &crown, double (*ground)(void *, double, double), void *user,
                        std::vector<float> &out, std::vector<float> &dist) const {
  out.clear();
  dist.clear();
  Count_ = 0;
  Cleared_ = 0;
  AboveLine_ = 0;
  TooSteep_ = 0;
  HighestAsl_ = -1.0e30;
  const VegetationTemplates::Row *rows = veg.Rows();
  if (!rows) return;
  const int nrows = (int)(veg.RowBytes() / sizeof(VegetationTemplates::Row));
  const int r = (int)std::ceil(radiusM / kCellM);
  const int ci = (int)std::floor(eyeE / kCellM), cj = (int)std::floor(eyeN / kCellM);
  const AlpineLimit &limit = veg.Limit();
  /* THE DEM'S OWN POST SPACING, so the stencil neither smooths the wall nor interpolates its own
   * bilinear filter: fb_stream_ground reads z13, whose post is 40075017*cos(lat)/(2^13*256) m —
   * 12.93 m at 47.4 deg. Measured over the Mandlwand and the Zugspitze north face, z13 carries the
   * slope: from z13 to z15 the p90 moves at most 1.9 deg (vegetation.json alpineLimit.slopeSource). */
  const double postM = 40075017.0 * std::cos(camLatDeg * 3.14159265358979 / 180.0) / (8192.0 * 256.0);

  for (int j = cj - r; j <= cj + r; j++) {
    for (int i = ci - r; i <= ci + r; i++) {
      const uint32_t h = Hash(i, j, 0x5eedu);
      const double e = ((double)i + 0.25 + 0.5 * U(h)) * kCellM;
      const double n = ((double)j + 0.25 + 0.5 * U(h >> 8)) * kCellM;
      const double de = e - eyeE, dn = n - eyeN;
      if (de * de + dn * dn > radiusM * radiusM) continue;

      const int tpl = cls.Evaluate(e, n, nullptr, nullptr);
      if (tpl < 0 || tpl >= nrows) continue;
      const float perM2 = rows[tpl].Edge[2];
      if (perM2 <= 0.0f) continue;
      /* ITS OWN DRAW, with the full 24 bits. `U(h >> 16)` left only 16 and so delivered at most
       * 0.0039: the acceptance threshold sat above EVERY declared density, so every cell with
       * perM2 > 0 was accepted. The 219 240 stands within 900 m were not 0.09/m2, they were
       * "everything that is not field, water or sealed surface". */
      if (U(Hash(i, j, 0xd3adu)) > (float)(perM2 * kCellM * kCellM)) continue;

      const double gz = ground ? ground(user, e, n) : eyeAsl;
      if (gz <= -1.0e7) continue;
      /* THE DEM DECIDES WHERE NOTHING STANDS, and it decides it in one draw against one fraction —
       * two draws would let a stand survive the treeline only to be counted again against the wall. */
      const double woody = limit.WoodyFraction(camLatDeg, gz, e, n);
      double steep = 0.0;
      if (woody > 0.0 && ground) {
        const double ze = ground(user, e + postM, n), zw = ground(user, e - postM, n);
        const double zn = ground(user, e, n + postM), zs = ground(user, e, n - postM);
        if (ze > -1.0e7 && zw > -1.0e7 && zn > -1.0e7 && zs > -1.0e7) {
          const double dzde = (ze - zw) / (2.0 * postM), dzdn = (zn - zs) / (2.0 * postM);
          steep = limit.BareBySlope(std::atan(std::sqrt(dzde * dzde + dzdn * dzdn)) * 57.29577951308232,
                                    (double)rows[tpl].Edge[3]);
        }
      }
      if (woody <= 0.0) { AboveLine_++; continue; }
      if (steep >= 1.0) { TooSteep_++; continue; }
      if ((double)U(Hash(i, j, 0xa17eu)) >= woody * (1.0 - steep)) continue;
      /* Its own draw, not another slice of `h`: place, yaw and density already share bits there, and
       * a size correlated with yaw stripes the forest into bands. */
      const float size = SizeFactor(Hash(i, j, 0x9e37u), sizeSigma);
      if (crown.HeightM > 0.0f && crown.HalfWidth > 0.0f) {
        const double hm = (double)crown.HeightM * (double)size;
        const double half = hm * (double)crown.HalfWidth;
        if (eyeAsl > gz + hm * (double)crown.Bottom && eyeAsl < gz + hm * (double)crown.Top &&
            de * de + dn * dn < half * half) {
          Cleared_++;
          continue;
        }
      }
      /* RELATIVE TO THE CAMERA, because the shader places the stand on the ECEF axes at the eye: an
       * absolute ENU value would be a kilometre-sized offset there. The search itself runs absolute —
       * class and terrain are properties of the place, not of the viewer. */
      if (gz > HighestAsl_) HighestAsl_ = gz;
      out.push_back((float)(e - eyeE)); out.push_back((float)(n - eyeN));
      out.push_back((float)(gz - eyeAsl));
      out.push_back(U(h >> 20) * 6.2831853f);
      out.push_back(size);
      const double dz = gz - eyeAsl;
      dist.push_back((float)std::sqrt(de * de + dn * dn + dz * dz));
      Count_++;
    }
  }

  const size_t n = dist.size();
  std::vector<uint32_t> order(n);
  for (size_t k = 0; k < n; k++) order[k] = (uint32_t)k;
  std::sort(order.begin(), order.end(),
            [&dist](uint32_t a, uint32_t c) { return dist[a] < dist[c]; });
  std::vector<float> so(out.size()), sd(n);
  for (size_t k = 0; k < n; k++) {
    sd[k] = dist[order[k]];
    for (int c = 0; c < kStandFloats; c++) {
      so[k * (size_t)kStandFloats + (size_t)c] = out[(size_t)order[k] * (size_t)kStandFloats + (size_t)c];
    }
  }
  out.swap(so);
  dist.swap(sd);
}

} // namespace outshine::World
