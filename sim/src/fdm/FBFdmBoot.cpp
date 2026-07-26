#include "FBFdmBoot.h"

namespace FlightBox {

/* The one place an FBFdm comes into existence. `new` rather than make_unique because FBFdm's
 * constructor is private to this class (the IC gate, see FBFdmBoot.h) — ownership is in the unique_ptr
 * before anything can throw. */
std::unique_ptr<FBFdm> FBFdmBoot::Spawn(const FBFdmSpawn &spawn) {
  std::unique_ptr<FBFdm> fdm(new FBFdm());
  if (!fdm->Load(spawn)) return nullptr;   /* an FBFdm that exists is always a loaded one */
  return fdm;
}

} // namespace FlightBox
