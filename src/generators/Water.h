#ifndef OUTSHINE_GENERATORS_WATER_H
#define OUTSHINE_GENERATORS_WATER_H

#include "FeatureField.h"
#include "Making.h"
#include "WaterDepth.h"

namespace outshine::Generators {

class Water : public Making {
public:
  [[nodiscard]] static WaterDepth
  DepthAt(const Ground &ground, double eastM, double northM) noexcept;

  enum Note { Surfaces, Untested, LevelBelowGround, DeepestM, kNotes };

  [[nodiscard]] Span<const char *const> NoteNames() const noexcept override;

  void Occupy(const Ground &ground, Yield &yield) const noexcept override;
  [[nodiscard]] bool
  At(const Ground &ground, double eastM, double northM, Body *out) const noexcept override;
  [[nodiscard]] uint32_t Proposes(double areaM2) const noexcept override;
};

} // namespace outshine::Generators
#endif
