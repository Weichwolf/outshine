/* The guided catalogue's registration with FBModuleRegistry — one name per round, all of them the same
 * class (modules/missile/FBMissileModule). Its own translation unit for the same reason the F-16's and
 * the stores' are: this is the only file outside the registry that names a concrete module type.
 *
 * The split with FBStoreModuleRegistration is the catalogue's own Guided flag, read in ONE place each,
 * so a new entry lands in the right module by being described correctly rather than by being remembered
 * in two lists. */
#include "FBMissileModule.h"
#include "FBModuleRegistry.h"
#include "FBStore.h"

namespace FlightBox {

void FBRegisterMissileModules() {
  for (const FBStoreSpec *spec : kStoreCatalogue) {
    if (!spec->Guided) continue;
    FBModuleRegistry::Register(spec->Key, [spec]() -> std::unique_ptr<FBModule> {
      return std::make_unique<FBMissileModule>(*spec);
    });
  }
}

} // namespace FlightBox
