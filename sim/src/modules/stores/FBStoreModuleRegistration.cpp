/* One name per unguided store, all the same class; the only file outside the registry that names it. */
#include "FBModuleRegistry.h"
#include "FBStore.h"
#include "FBStoreModule.h"

namespace FlightBox {

void FBRegisterStoreModules() {
  for (const FBStoreSpec *spec : kStoreCatalogue) {
    if (spec->Guided) continue;   /* a guided round is modules/missile's, not this class's */
    FBModuleRegistry::Register(spec->Key, [spec]() -> std::unique_ptr<FBModule> {
      return std::make_unique<FBStoreModule>(*spec);
    });
  }
}

} // namespace FlightBox
