/* FlightBox — FBUnitRegistry: WHO is in the world, as a plain list of borrowed units. It used to be a
 * member vector of world/FBWorld, which put the cast of a mission on the RENDERER's side of the
 * library split: fb-gym does not link world/ at all and therefore handed every module a null world, so
 * no simulated sensor could ever have seen another unit in the one client the mission loop actually
 * runs in. The list is simulation state (it exists with or without a camera), so it lives here, in the
 * core library, and FBWorld only BORROWS it for the drawing side.
 *
 * Borrowed, never owned: the client owns its units/FBSimUnit list and registers each one once at boot.
 * Registration order is the mission's declaration order and never changes during a run, so every
 * consumer walks the same sequence every tick — the determinism the threaded step phase depends on.
 *
 * READ-ONLY BY CONSTRUCTION (CLAUDE.md "Kein Cheaten"): entries are `const FBUnit *`, so a system that
 * holds this registry can observe identity and the PUBLISHED pose/signature of the last completed tick
 * (FBUnit's snapshot contract) and nothing else — it cannot step, steer or write another unit. Who is
 * allowed to hold it at all is a separate, narrower rule: only a simulated SENSOR gets it (today
 * systems/FBDatalinkSystem, tomorrow the radar), never the pilot, which is why it travels as a Run()
 * argument down the module's system-cycling path instead of being a member anyone can reach. */
#ifndef FBUNITREGISTRY_H
#define FBUNITREGISTRY_H

#include <cstddef>
#include <vector>

namespace FlightBox {

class FBUnit;

class FBUnitRegistry {
public:
  void Register(const FBUnit *unit) { Units_.push_back(unit); }
  void Clear() { Units_.clear(); }

  const std::vector<const FBUnit *> &Units() const { return Units_; }
  size_t Size() const { return Units_.size(); }

private:
  std::vector<const FBUnit *> Units_;
};

} // namespace FlightBox
#endif
