#ifndef OUTSHINE_GENERATORS_ROAD_TRANSPORT_H
#define OUTSHINE_GENERATORS_ROAD_TRANSPORT_H

#include <span>
#include "Making.h"

namespace outshine::Generators {

class Transport : public Making {
public:
  enum Note { Ways, WidestM, kNotes };

  [[nodiscard]] std::span<const char *const> NoteNames() const noexcept override;

  void Occupy(const Ground &ground, Yield &yield) const noexcept override;

  [[nodiscard]] std::string_view Called() const noexcept override { return "road"; }
};

}
#endif
