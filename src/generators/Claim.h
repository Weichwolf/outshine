#ifndef OUTSHINE_GENERATORS_CLAIM_H
#define OUTSHINE_GENERATORS_CLAIM_H

#include <cstddef>
#include <optional>

#include "BodyId.h"

namespace outshine::Generators {

class Claim {
public:
  enum class Outcome { Placed, Occupied, Outside, Full };
  static constexpr size_t kOutcomes = 4;

  static Claim Of(BodyId id) { return Claim(Outcome::Placed, id); }
  static Claim Refused(Outcome why) { return Claim(why, std::nullopt); }

  [[nodiscard]] Outcome Why() const noexcept { return Why_; }

  [[nodiscard]] bool TryId(BodyId *out) const noexcept {
    if (!Id_) return false;
    *out = *Id_;
    return true;
  }

private:
  Claim(Outcome why, std::optional<BodyId> id) : Id_(id), Why_(why) {}

  std::optional<BodyId> Id_;
  Outcome Why_;
};

}
#endif
