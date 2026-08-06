/* THE ONE HUD THE ENGINE DRAWS: it holds a deck of declared rows (systems/FBHudDecl.h) and walks it
 * once per frame. There is no per-title HUD class any more and there must not be one again — a title
 * that needs a symbol this cannot draw needs a new ELEMENT KIND here, which every other title then
 * has too, and the missing kind is the measurement (doc/mods.md §2).
 *
 * The deck is MOVED in at construction, like the aircraft catalogue: a client loads the mod's file,
 * hands over the rows, and core never looks for a path. */
#ifndef FBDECLAREDHUD_H
#define FBDECLAREDHUD_H

#include <utility>

#include "FBDisplaySystem.h"
#include "FBHudDecl.h"

namespace FlightBox::Systems {

class FBDeclaredHud : public FBDisplaySystem {
public:
  explicit FBDeclaredHud(FBHudDeck deck) : Deck_(std::move(deck)) {}

  const FBHudDeck &Deck() const { return Deck_; }

  void BuildHud(const FBState &state, const FBHudEnv &env, FBHudGeometry &out) const override;

private:
  FBHudDeck Deck_;
};

} // namespace FlightBox::Systems
#endif
