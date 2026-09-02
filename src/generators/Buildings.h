#ifndef OUTSHINE_GENERATORS_BUILDINGS_H
#define OUTSHINE_GENERATORS_BUILDINGS_H

#include <span>
#include "FeatureField.h"
#include "Earth.h"
#include "Making.h"

namespace outshine::Generators {

class Buildings : public Making {
public:
  explicit Buildings(ContactMaterial contact);

  enum Note { Footprints, Roofless, HighestRoofAglM, kNotes };

  [[nodiscard]] std::span<const char *const> NoteNames() const noexcept override;

  void Occupy(const Ground &ground, Yield &yield) const noexcept override;

private:
  [[nodiscard]] static const FeatureField::Feature *Over(const Ground &ground,
                                                         EastNorth at) noexcept;

  ContactMaterial Contact_;
};

} // namespace outshine::Generators
#endif
