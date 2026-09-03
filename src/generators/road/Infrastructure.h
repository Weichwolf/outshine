#ifndef OUTSHINE_GENERATORS_ROAD_INFRASTRUCTURE_H
#define OUTSHINE_GENERATORS_ROAD_INFRASTRUCTURE_H

#include <span>
#include <optional>

#include "FeatureField.h"
#include "Earth.h"
#include "Making.h"

namespace outshine::Generators {

class Infrastructure : public Making {
public:
  struct Made {
    int32_t CoverRow = -1;
    float WidthM = 0.0f;
    double SurfaceAslM = 0.0;
  };

  [[nodiscard]] static std::optional<Made> MadeAt(const Ground &ground, EastNorth at) noexcept;

  enum Note { Ways, WidestM, kNotes };

  [[nodiscard]] std::span<const char *const> NoteNames() const noexcept override;

  void Occupy(const Ground &ground, Yield &yield) const noexcept override;

  [[nodiscard]] std::string_view Called() const noexcept override { return "road"; }
};

} // namespace outshine::Generators
#endif
