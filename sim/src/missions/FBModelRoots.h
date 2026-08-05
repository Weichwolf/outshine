/* ONE model root, and it is NOT the submodule: the MOD flies its own copy (FBMod.h), the pinned
 * submodule is the upstream BASIS `make -C sim verify-models` diffs against, and every deviation is a
 * named entry in the mod's MODEL-DELTAS.md. A model loaded from the submodule could carry no
 * correction; one loaded from a copy of unknown provenance would be no reference.
 *
 * Lives in missions/ because only missions/ boots an airframe: nothing under systems/ or modules/
 * reaches a model path, any more than it reaches an initial condition.
 * doc/units-and-missions.md, Abschnitt 11. */
#ifndef FBMODELROOTS_H
#define FBMODELROOTS_H

#include <string>

namespace FlightBox::Missions {

struct FBModelRoots {
  std::string Aircraft;    /* FlightBox's model root: one directory per model */
  /* Everything else this client can load from disk by NAME — today the baked weather fixture a mission's
   * `wx fixture` line names. Empty = this client has no filesystem to resolve one against (the browser). */
  std::string Assets;
  /* The scenario's AIRCRAFT CATALOGUE — the rows the engine does not own (FBCatalogueBoot.h). A path
   * and not a directory: it is one declaration, and the manifest names it whole. */
  std::string Catalogue;
};

} // namespace FlightBox::Missions
#endif
