/* ONE model root, and it is NOT the submodule: FlightBox flies its own copy under sim/assets/aircraft,
 * the pinned submodule is the upstream BASIS `make -C sim verify-models` diffs against, and every
 * deviation is a named entry in sim/assets/MODEL-DELTAS.md. A model loaded from the submodule could
 * carry no correction; one loaded from a copy of unknown provenance would be no reference.
 *
 * Lives in app/ because only app/ boots an airframe: nothing under systems/ or modules/ reaches a model
 * path, any more than it reaches an initial condition.
 * doc/flightbox/units-and-missions.md, Abschnitt 11. */
#ifndef FBMODELROOTS_H
#define FBMODELROOTS_H

#include <string>

namespace FlightBox::Missions {

struct FBModelRoots {
  std::string Aircraft;    /* FlightBox's model root: one directory per model */
  /* Everything else this client can load from disk by NAME — today the baked weather fixture a mission's
   * `wx fixture` line names. Empty = this client has no filesystem to resolve one against (the browser). */
  std::string Assets;
};

/* One definition for every client that runs from sim/, instead of literals that could drift apart. */
inline FBModelRoots FBNativeModelRoots() { return FBModelRoots{"assets/aircraft", "assets"}; }

} // namespace FlightBox::Missions
#endif
