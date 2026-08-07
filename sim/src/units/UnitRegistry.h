/* outshine — UnitRegistry: WHO is in the world, as a plain list of borrowed units. Simulation state,
 * so it lives in the core library and World only BORROWS it for the drawing side. Registration order
 * is the mission's declaration order and never changes — the determinism the threaded step phase needs.
 * Entries are `const Unit *`, so a holder can observe the published snapshot and nothing else; only a
 * simulated SENSOR gets one, which is why it travels as a Run() argument and never as a member. */
#ifndef UNITREGISTRY_H
#define UNITREGISTRY_H

#include <cstddef>
#include <vector>

namespace outshine::Units {

class Unit;

class UnitRegistry {
public:
  void Register(const Unit *unit) { Units_.push_back(unit); }
  void Clear() { Units_.clear(); }

  const std::vector<const Unit *> &Units() const { return Units_; }
  size_t Size() const { return Units_.size(); }

private:
  std::vector<const Unit *> Units_;
};

} // namespace outshine::Units
#endif
