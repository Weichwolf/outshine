#ifndef BUILDINGS_H
#define BUILDINGS_H

#include "FeatureField.h"
#include "Generator.h"

namespace outshine::Generators {

class Buildings : public Generator {
public:
  explicit Buildings(ContactMaterial contact);

  enum Note { Footprints, Roofless, HighestRoofAglM, kNotes };
  Span<const char *const> NoteNames() const noexcept override;

  void Occupy(const Ground &ground, Yield &yield) const noexcept override;
  [[nodiscard]] bool At(const Ground &ground, double eastM, double northM,
                        Body *out) const noexcept override;
  [[nodiscard]] uint32_t Proposes(double areaM2) const noexcept override;

private:

  const FeatureField::Feature *Over(const Ground &ground, double eastM,
                                    double northM) const noexcept;

  ContactMaterial Contact_;
};

}
#endif
