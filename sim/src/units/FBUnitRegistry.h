/* FlightBox — FBUnitRegistry: WHO is in the world, as a plain list of borrowed units. Simulation state,
 * so it lives in the core library and FBWorld only BORROWS it for the drawing side. Registration order
 * is the mission's declaration order and never changes — the determinism the threaded step phase needs.
 * Entries are `const FBUnit *`, so a holder can observe the published snapshot and nothing else; only a
 * simulated SENSOR gets one, which is why it travels as a Run() argument and never as a member. */
#ifndef FBUNITREGISTRY_H
#define FBUNITREGISTRY_H

#include <cstddef>
#include <vector>

namespace FlightBox::Units {

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

} // namespace FlightBox::Units
#endif
