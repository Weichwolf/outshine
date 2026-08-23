#ifndef OUTSHINE_GENERATORS_INFRASTRUCTURE_H
#define OUTSHINE_GENERATORS_INFRASTRUCTURE_H

#include <optional>

#include "FeatureField.h"
#include "Generator.h"

namespace outshine::Generators {

class Infrastructure : public Generator {
public:

  struct Made {
    int32_t CoverRow = -1;
    float WidthM = 0.0f;
    double SurfaceAslM = 0.0;
  };
  [[nodiscard]] std::optional<Made> MadeAt(const Ground &ground, double eastM,
                                           double northM) const noexcept;

  enum Note { Ways, WidestM, kNotes };
  [[nodiscard]] Span<const char *const> NoteNames() const noexcept override;

  void Occupy(const Ground &ground, Yield &yield) const noexcept override;
  [[nodiscard]] bool At(const Ground &ground, double eastM, double northM,
                        Body *out) const noexcept override;
  [[nodiscard]] uint32_t Proposes(double areaM2) const noexcept override;
};

}
#endif
