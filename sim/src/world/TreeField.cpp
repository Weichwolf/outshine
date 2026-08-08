#include "TreeField.h"

#include "ClassField.h"
#include "VegetationTemplates.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace outshine::World {

namespace {

/* Eine Zelle, ein Wurf — damit ist die Streuung eine Funktion der Zelle und nicht einer Reihenfolge.
 * Die Kante muss DEUTLICH unter dem mittleren Standabstand liegen: bei 11.09 m2 nimmt die dichteste
 * deklarierte Klasse (Nadelwald, 0.033/m2) 36.6 % der Zellen an, zwei von drei bleiben leer, und
 * genau das verhindert, dass das Gitter als Reihe liest. Bei einer Kante um den Standabstand herum
 * waere die Annahmewahrscheinlichkeit 1 und der Wald ein Raster. */
constexpr double kCellM = 3.33;

inline uint32_t Hash(int32_t i, int32_t j, uint32_t salt) {
  uint32_t h = (uint32_t)i * 0x8da6b343u ^ (uint32_t)j * 0xd8163841u ^ salt * 0xcb1ab31fu;
  h ^= h >> 15; h *= 0x2c1b3c6du; h ^= h >> 12; h *= 0x297a2d39u; h ^= h >> 15;
  return h;
}
inline float U(uint32_t h) { return (float)(h & 0xFFFFFFu) / 16777216.0f; }
inline float U16(uint32_t h) { return (float)(h & 0xFFFFu) / 65536.0f; }

/* Dreieckig statt normal, weil die Verteilung bei mu +- 2.449 sigma HART endet: ueber 166 000 Staende
 * liefert eine normale Ziehung sicher einen 4-sigma-Baum, und der ist im Bild ein Fehler, keine
 * Streuung. 2.4494897 = 1/sqrt(2/12), die Standardabweichung der Summe zweier Gleichverteilter. */
inline float SizeFactor(uint32_t h, float sigma) {
  return 1.0f + sigma * (U16(h) + U16(h >> 16) - 1.0f) * 2.4494897f;
}

}  // namespace

void TreeField::Scatter(const ClassField &cls, const VegetationTemplates &veg, double radiusM,
                        double eyeE, double eyeN, double eyeAsl, float sizeSigma,
                        const Crown &crown, double (*ground)(void *, double, double), void *user,
                        std::vector<float> &out, std::vector<float> &dist) const {
  out.clear();
  dist.clear();
  Count_ = 0;
  Cleared_ = 0;
  const VegetationTemplates::Row *rows = veg.Rows();
  if (!rows) return;
  const int nrows = (int)(veg.RowBytes() / sizeof(VegetationTemplates::Row));
  const int r = (int)std::ceil(radiusM / kCellM);
  const int ci = (int)std::floor(eyeE / kCellM), cj = (int)std::floor(eyeN / kCellM);

  for (int j = cj - r; j <= cj + r; j++) {
    for (int i = ci - r; i <= ci + r; i++) {
      const uint32_t h = Hash(i, j, 0x5eedu);
      const double e = ((double)i + 0.25 + 0.5 * U(h)) * kCellM;
      const double n = ((double)j + 0.25 + 0.5 * U(h >> 8)) * kCellM;
      const double de = e - eyeE, dn = n - eyeN;
      if (de * de + dn * dn > radiusM * radiusM) continue;

      const int tpl = cls.ClassAtEnu(e, n, nullptr, nullptr);
      if (tpl < 0 || tpl >= nrows) continue;
      const float perM2 = rows[tpl].Edge[2];
      if (perM2 <= 0.0f) continue;
      /* EIGENER WURF mit vollen 24 Bit. `U(h >> 16)` liess nur 16 Bit uebrig und lieferte damit
       * hoechstens 0.0039: die Annahmeschwelle lag ueber JEDER deklarierten Dichte, also wurde jede
       * Zelle mit perM2 > 0 angenommen. Die 219 240 Staende in 900 m waren nicht 0.09/m2, sie waren
       * "alles, was kein Acker, Wasser oder Siegel ist". */
      if (U(Hash(i, j, 0xd3adu)) > (float)(perM2 * kCellM * kCellM)) continue;

      const double gz = ground ? ground(user, e, n) : eyeAsl;
      if (gz <= -1.0e7) continue;
      /* Eigener Wurf, nicht ein weiterer Schnitt aus `h`: Ort, Gierung und Dichte teilen sich dort
       * schon Bits, und eine Groesse, die mit der Gierung korreliert, streift den Wald in Baendern. */
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
      /* RELATIV ZUR KAMERA, weil der Stand im Shader auf die ECEF-Achsen am Augpunkt gelegt wird: ein
       * absoluter ENU-Wert waere dort ein Versatz von Kilometern. Die Suche laeuft dagegen absolut,
       * denn Klasse und Gelaende sind Eigenschaften des Ortes und nicht des Betrachters. */
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
