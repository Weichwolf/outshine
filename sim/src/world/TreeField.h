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
  /* east, north, HoeheM, Gierung — je Baum vier Werte, wie TreeStage sie erwartet. */
  void Scatter(const ClassField &cls, const VegetationTemplates &veg, double radiusM,
               double eyeE, double eyeN, std::vector<float> &out) const;

  long Count() const { return Count_; }

private:
  mutable long Count_ = 0;
};

} // namespace outshine::World
#endif
