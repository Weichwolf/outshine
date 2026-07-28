/* One name per guided round, all the same class. The split with FBStoreModuleRegistration is the
 * catalogue's own Guided flag, read in ONE place each, so a new entry lands in the right module by
 * being described correctly rather than by being remembered in two lists. */
#include "FBMissileModule.h"
#include "FBModuleRegistry.h"
#include "FBStore.h"

namespace FlightBox::Modules {

void FBRegisterMissileModules() {
  for (const FBStoreSpec *spec : kStoreCatalogue) {
    if (!spec->Guided) continue;
    FBModuleRegistry::Register(spec->Key, [spec]() -> std::unique_ptr<FBModule> {
      return std::make_unique<FBMissileModule>(*spec);
    });
  }
}

} // namespace FlightBox::Modules
