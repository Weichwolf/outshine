/* WHAT THE SINK DID WITH A BODY, and each cause under its own name. One sentinel for several states
 * has cost this tree a forest twice: a generator that cannot tell "the buffer is full" from "a trunk
 * already stands here" either stops on a conflict or runs a whole region into a sink that is
 * finished. `Outside` is neither — it is a defect in the caller, and it counts as itself. */
#ifndef CLAIM_H
#define CLAIM_H

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
  /* The claimed body's index, written only where a claim succeeded. There is no id to mint for a
   * refusal, which is why this carries the one it was handed rather than a number of its own. */
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

} // namespace outshine::Generators
#endif
