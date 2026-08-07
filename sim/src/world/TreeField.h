/* Wo Baeume stehen. Eine FUNKTION des Ortes, keine Liste: dieselbe Rechnung beantwortet "was steht in
 * dieser Zelle" fuer das Bild und "steht hier ein Stamm" fuer einen Koerper, und keine Seite haelt
 * dafuer Speicher. */
#ifndef TREEFIELD_H
#define TREEFIELD_H

#include <cstdint>
#include <vector>

namespace outshine::World {

class ClassField;
class VegetationTemplates;

class TreeField {
public:
  /* east, north, Fuss ueber der Augenhoehe, Gierung, Groessenfaktor — je Baum fuenf Werte, wie
   * TreeStage sie erwartet. `sizeSigma` ist die relative Streuung der Bestandeshoehe der Art. */
  /* `ground` beantwortet die Hoehe an einem Ort in ENU-Metern; ohne sie saessen alle Staende auf der
   * Augenhoehe der Kamera, also am Hang im Fels oder in der Luft. */
  /* `dist` comes out beside `out` and `out` comes out SORTED BY IT, near to far: a level of detail is
   * then a contiguous range of instances and choosing it is a binary search instead of a per-frame
   * pass over the field (doc/render/lod.md). */
  void Scatter(const ClassField &cls, const VegetationTemplates &veg, double radiusM,
               double eyeE, double eyeN, double eyeAsl, float sizeSigma,
               double (*ground)(void *, double, double), void *user,
               std::vector<float> &out, std::vector<float> &dist) const;

  static constexpr int kStandFloats = 5;

  long Count() const { return Count_; }

private:
  mutable long Count_ = 0;
};

} // namespace outshine::World
#endif
