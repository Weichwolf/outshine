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

const char *TreeField::RefusalName(Refusal r) {
  static const char *const kNames[(int)Refusal::Count] = {
      "placed", "outsideRadius", "noTemplate", "zeroDensity", "densityDraw", "groundPending",
      "noGround", "aboveTreeline", "tooSteep", "woodyDraw", "inCrown"};
  return kNames[(int)r];
}

void TreeField::Scatter(const ClassStructure &cls, const VegetationTemplates &veg, double radiusM,
                        double eyeE, double eyeN, double eyeAsl, double camLatDeg, float sizeSigma,
                        const Crown &crown, const GroundOracle &ground,
                        std::vector<float> &out, std::vector<float> &dist) const {
  out.clear();
  dist.clear();
  for (long &c : Counts_) c = 0;
  HighestAsl_ = -1.0e30;
  const VegetationTemplates::Row *rows = veg.Rows();
  if (!rows) return;
  const int nrows = (int)(veg.RowBytes() / sizeof(VegetationTemplates::Row));
  const int r = (int)std::ceil(radiusM / kCellM);
  const int ci = (int)std::floor(eyeE / kCellM), cj = (int)std::floor(eyeN / kCellM);
  const AlpineLimit &limit = veg.Limit();

  struct Stand { double E, N, GroundAsl, DistSq; float Yaw, Size; };

  /* ONE CELL, ONE ANSWER. Every way out of this is a Refusal, so the counts partition the disc by
   * construction and a stand that is missing has a name. */
  const auto consider = [&](int i, int j, Stand &st) -> Refusal {
    const uint32_t h = Hash(i, j, 0x5eedu);
    st.E = ((double)i + 0.25 + 0.5 * U(h)) * kCellM;
    st.N = ((double)j + 0.25 + 0.5 * U(h >> 8)) * kCellM;
    const double de = st.E - eyeE, dn = st.N - eyeN;
    st.DistSq = de * de + dn * dn;
    st.Yaw = U(h >> 20) * 6.2831853f;
    if (st.DistSq > radiusM * radiusM) return Refusal::OutsideRadius;

    const int tpl = cls.Evaluate(st.E, st.N, nullptr, nullptr);
    if (tpl < 0 || tpl >= nrows) return Refusal::NoTemplate;
    const float perM2 = rows[tpl].Edge[2];
    if (perM2 <= 0.0f) return Refusal::ZeroDensity;
    /* ITS OWN DRAW, with the full 24 bits. `U(h >> 16)` left only 16 and so delivered at most
     * 0.0039: the acceptance threshold sat above EVERY declared density, so every cell with
     * perM2 > 0 was accepted. The 219 240 stands within 900 m were not 0.09/m2, they were
     * "everything that is not field, water or sealed surface". */
    if (U(Hash(i, j, 0xd3adu)) > (float)(perM2 * kCellM * kCellM)) return Refusal::DensityDraw;

    if (!ground.At) {
      st.GroundAsl = eyeAsl;
    } else {
      const GroundSample g = ground.At(ground.User, st.E, st.N);
      if (!g.TryAslM(&st.GroundAsl))
        return g.Where() == GroundSample::State::Pending ? Refusal::GroundPending : Refusal::NoGround;
    }
    /* THE DEM DECIDES WHERE NOTHING STANDS, and it decides it in one draw against one fraction —
     * two draws would let a stand survive the treeline only to be counted again against the wall. */
    const double woody = limit.WoodyFraction(camLatDeg, st.GroundAsl, st.E, st.N);
    double steep = 0.0;
    if (woody > 0.0 && ground.At) {
      const double p = ground.PostM;
      double ze = 0.0, zw = 0.0, zn = 0.0, zs = 0.0;
      if (ground.At(ground.User, st.E + p, st.N).TryAslM(&ze) &&
          ground.At(ground.User, st.E - p, st.N).TryAslM(&zw) &&
          ground.At(ground.User, st.E, st.N + p).TryAslM(&zn) &&
          ground.At(ground.User, st.E, st.N - p).TryAslM(&zs)) {
        const double dzde = (ze - zw) / (2.0 * p), dzdn = (zn - zs) / (2.0 * p);
        steep = limit.BareBySlope(std::atan(std::sqrt(dzde * dzde + dzdn * dzdn)) * 57.29577951308232,
                                  (double)rows[tpl].Edge[3]);
      }
    }
    if (woody <= 0.0) return Refusal::AboveTreeline;
    if (steep >= 1.0) return Refusal::TooSteep;
    if ((double)U(Hash(i, j, 0xa17eu)) >= woody * (1.0 - steep)) return Refusal::WoodyDraw;
    /* Its own draw, not another slice of `h`: place, yaw and density already share bits there, and
     * a size correlated with yaw stripes the forest into bands. */
    st.Size = SizeFactor(Hash(i, j, 0x9e37u), sizeSigma);
    if (crown.HeightM > 0.0f && crown.HalfWidth > 0.0f) {
      const double hm = (double)crown.HeightM * (double)st.Size;
      const double half = hm * (double)crown.HalfWidth;
      if (eyeAsl > st.GroundAsl + hm * (double)crown.Bottom &&
          eyeAsl < st.GroundAsl + hm * (double)crown.Top && st.DistSq < half * half)
        return Refusal::InCrown;
    }
    return Refusal::Placed;
  };

  for (int j = cj - r; j <= cj + r; j++) {
    for (int i = ci - r; i <= ci + r; i++) {
      Stand st{};
      const Refusal why = consider(i, j, st);
      Counts_[(int)why]++;
      if (why != Refusal::Placed) continue;
      /* RELATIVE TO THE CAMERA, because the shader places the stand on the ECEF axes at the eye: an
       * absolute ENU value would be a kilometre-sized offset there. The search itself runs absolute —
       * class and terrain are properties of the place, not of the viewer. */
      if (st.GroundAsl > HighestAsl_) HighestAsl_ = st.GroundAsl;
      const double dz = st.GroundAsl - eyeAsl;
      out.push_back((float)(st.E - eyeE));
      out.push_back((float)(st.N - eyeN));
      out.push_back((float)dz);
      out.push_back(st.Yaw);
      out.push_back(st.Size);
      dist.push_back((float)std::sqrt(st.DistSq + dz * dz));
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
