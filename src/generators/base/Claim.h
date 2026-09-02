#ifndef OUTSHINE_GENERATORS_BASE_CLAIM_H
#define OUTSHINE_GENERATORS_BASE_CLAIM_H

#include <cstddef>
#include <optional>

#include "BodyId.h"

namespace outshine::Generators {

class Claim {
public:
  enum class Outcome { Placed, Occupied, Outside, Full };
  static constexpr size_t kOutcomes = 4;

  static Claim Placed() { return Claim(Outcome::Placed); }

  static Claim Refused(Outcome why) { return Claim(why); }

  [[nodiscard]] Outcome Why() const noexcept { return Why_; }

private:
  explicit Claim(Outcome why) : Why_(why) {}

  Outcome Why_;
};

} // namespace outshine::Generators
#endif
