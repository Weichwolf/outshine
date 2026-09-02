#ifndef OUTSHINE_GENERATORS_INFRASTRUCTURE_H
#define OUTSHINE_GENERATORS_INFRASTRUCTURE_H

#include <optional>

#include "FeatureField.h"
#include "Making.h"

namespace outshine::Generators {

class Infrastructure : public Making {
public:
  struct Made {
    int32_t CoverRow = -1;
    float WidthM = 0.0f;
    double SurfaceAslM = 0.0;
  };

  [[nodiscard]] static std::optional<Made>
  MadeAt(const Ground &ground, double eastM, double northM) noexcept;

  enum Note { Ways, WidestM, kNotes };

  [[nodiscard]] Span<const char *const> NoteNames() const noexcept override;

  void Occupy(const Ground &ground, Yield &yield) const noexcept override;
};

} // namespace outshine::Generators
#endif
