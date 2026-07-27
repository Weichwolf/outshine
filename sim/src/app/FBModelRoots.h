/* FlightBox — FBModelRoots: where JSBSim models come from, as one value the client fills in once.
 *
 * ONE ROOT, and it is not the submodule. Everything FlightBox flies lives under sim/assets/aircraft, one
 * self-contained directory per model (its .xml plus its own engine/ and Systems/ — JSBSim's own
 * per-aircraft layout, which its loaders search before any shared path). The pinned submodule is the
 * upstream BASIS that `make -C sim verify-models` diffs those copies against; it is not a load path.
 *
 * WHY THE INVERSION (CLAUDE.md, Prinzip 1's Delta-Regel). The submodule used to BE the
 * aircraft root precisely because it was read-only, and sim/assets/aircraft existed only for what the
 * submodule does not carry. That cannot hold once a model may legitimately be corrected or extended: a
 * model loaded from the submodule cannot carry a correction, and a model loaded from a copy of unknown
 * provenance is no longer a reference. The third option is this one — FlightBox flies its own copy, the
 * submodule stays the untouched basis, and every deviation is a named, evidenced entry in
 * sim/assets/MODEL-DELTAS.md that the verify gate holds to.
 *     native/gym : "assets/aircraft"   (relative to sim/)
 *     WASM       : "/fb/aircraft"      (the embedded FS, see the wasm make target's --embed-file line)
 * Lives in app/ because only app/ boots an airframe (fdm/FBFdmBoot.h's gate): nothing under systems/ or
 * modules/ can reach a model path any more than it can reach an initial condition. */
#ifndef FBMODELROOTS_H
#define FBMODELROOTS_H

#include <string>

namespace FlightBox {

struct FBModelRoots {
  std::string Aircraft;    /* FlightBox's model root: one directory per model */
};

/* The native/gym root, one definition for every client that runs from sim/ (both apps and every test
 * harness) instead of string literals that could drift apart. */
inline FBModelRoots FBNativeModelRoots() { return FBModelRoots{"assets/aircraft"}; }

} // namespace FlightBox
#endif
