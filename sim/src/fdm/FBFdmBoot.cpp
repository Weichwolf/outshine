#include "FBFdmBoot.h"
#include "FBLog.h"
#include <exception>
#include <string>

namespace FlightBox {

/* `new` rather than make_unique because FBFdm's ctor is private to this class; the try covers the ONE
 * thing outside FBFdm::Load's own firewall — constructing the engine object itself. */
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
