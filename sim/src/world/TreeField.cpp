#include "TreeField.h"

#include "ClassField.h"
#include "VegetationTemplates.h"

#include <cmath>

namespace outshine::World {

namespace {

/* Die Zelle ist so gross, dass die dichteste deklarierte Klasse (Nadelwald, 0.09/m2) im Mittel EINEN
 * Baum je Zelle traegt: 1/0.09 = 11.1 m2, also 3.33 m Kante. Eine Zelle, ein Wurf — damit ist die
 * Streuung eine Funktion der Zelle und nicht einer Reihenfolge. */
constexpr double kCellM = 3.33;

inline uint32_t Hash(int32_t i, int32_t j, uint32_t salt) {
  uint32_t h = (uint32_t)i * 0x8da6b343u ^ (uint32_t)j * 0xd8163841u ^ salt * 0xcb1ab31fu;
  h ^= h >> 15; h *= 0x2c1b3c6du; h ^= h >> 12; h *= 0x297a2d39u; h ^= h >> 15;
  return h;
}
inline float U(uint32_t h) { return (float)(h & 0xFFFFFFu) / 16777216.0f; }

}  // namespace

void TreeField::Scatter(const ClassField &cls, const VegetationTemplates &veg, double radiusM,
                        double eyeE, double eyeN, std::vector<float> &out) const {
  out.clear();
  Count_ = 0;
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
      if (U(h >> 16) > (float)(perM2 * kCellM * kCellM)) continue;

      /* Hoehe aus der Art zu ziehen ist die naechste Runde; bis dahin die deklarierte Bestandshoehe
       * mit dem Streuungsanteil, den die Vorlage fuer Halme fuehrt. */
      const float base = perM2 > 0.06f ? 22.0f : 26.0f;
      const float hgt = base * (0.7f + 0.6f * U(h >> 4));
      out.push_back((float)e); out.push_back((float)n);
      out.push_back(hgt); out.push_back(U(h >> 20) * 6.2831853f);
      Count_++;
    }
  }
}

} // namespace outshine::World
