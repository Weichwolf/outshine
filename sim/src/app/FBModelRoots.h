/* FlightBox — FBModelRoots: where JSBSim models come from, as one value the client fills in once.
 *
 * WHY THERE ARE TWO. FlightBox has two KINDS of model. The aircraft are the pinned JSBSim submodule's
 * (vendor/jsbsim/aircraft), read-only and never patched — CLAUDE.md Prinzip 1. Everything the submodule
 * does NOT carry has to be written by us, and therefore cannot live there: the AIM-120 is the first, and
 * it sits in sim/assets/aircraft. Which of the two a module's model comes from is the MODULE's own
 * statement (FBModule::FdmModelVendored), never a guess from the name — so this struct is only the
 * per-client PATHS, resolved per link target:
 *     native/gym : "vendor/jsbsim/aircraft"  and  "assets/aircraft"   (relative to sim/)
 *     WASM       : "/jsbsim/aircraft"        and  "/fb/aircraft"      (the embedded FS, see the wasm
 *                                                                      make target's --embed-file lines)
 * Lives in app/ because only app/ boots an airframe (fdm/FBFdmBoot.h's gate): nothing under systems/ or
 * modules/ can reach a model path any more than it can reach an initial condition. */
#ifndef FBMODELROOTS_H
#define FBMODELROOTS_H

#include <string>

namespace FlightBox {

struct FBModelRoots {
  std::string Vendor;      /* the pinned JSBSim submodule's aircraft root */
  std::string FlightBox;   /* FlightBox's own model assets */

  /* The root for a model, given the module's own answer to "is it vendored". */
  const std::string &Of(bool vendored) const { return vendored ? Vendor : FlightBox; }
};

/* The native/gym pair, one definition for every client that runs from sim/ (both apps and every test
 * harness) instead of four string literals that could drift apart. */
inline FBModelRoots FBNativeModelRoots() { return FBModelRoots{"vendor/jsbsim/aircraft", "assets/aircraft"}; }

} // namespace FlightBox
#endif
