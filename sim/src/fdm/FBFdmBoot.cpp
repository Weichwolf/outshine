#include "FBFdmBoot.h"
#include "FBLog.h"
#include <exception>
#include <string>

namespace FlightBox {

/* The one place an FBFdm comes into existence. `new` rather than make_unique because FBFdm's
 * constructor is private to this class (the IC gate, see FBFdmBoot.h) — ownership is in the unique_ptr
 * before anything can throw. FBFdm::Load has its own exception firewall; the try here covers the ONE
 * thing outside it, constructing the engine object itself (FGFDMExec's ctor allocates its property root
 * and reads JSBSIM_* env vars). A failed spawn is always nullptr, never a terminated process. */
std::unique_ptr<FBFdm> FBFdmBoot::Spawn(const FBFdmSpawn &spawn) {
  try {
    std::unique_ptr<FBFdm> fdm(new FBFdm());
    if (!fdm->Load(spawn)) return nullptr;   /* an FBFdm that exists is always a loaded one */
    return fdm;
  } catch (const std::exception &e) {
    FBLog::Error("fdm", "spawn_exception", {{"aircraft", spawn.Aircraft}, {"what", std::string(e.what())}});
  } catch (...) {
    FBLog::Error("fdm", "spawn_exception", {{"aircraft", spawn.Aircraft}, {"what", "non-standard exception"}});
  }
  return nullptr;
}

} // namespace FlightBox
