#ifndef OUTSHINE_GENERATORS_WATER_H
#define OUTSHINE_GENERATORS_WATER_H

#include <span>
#include "FeatureField.h"
#include "Earth.h"
#include "Making.h"
#include "WaterDepth.h"

namespace outshine::Generators {

class Water : public Making {
public:
  [[nodiscard]] static WaterDepth DepthAt(const Ground &ground, EastNorth at) noexcept;

  enum Note { Surfaces, Untested, LevelBelowGround, DeepestM, kNotes };

  [[nodiscard]] std::span<const char *const> NoteNames() const noexcept override;

  void Occupy(const Ground &ground, Yield &yield) const noexcept override;
};

} // namespace outshine::Generators
#endif
