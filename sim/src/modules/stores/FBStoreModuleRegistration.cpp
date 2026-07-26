/* The catalogue's registration with FBModuleRegistry — one name per store, all of them the same class
 * (modules/stores/FBStoreModule). Its own translation unit for the same reason the F-16's is: this is
 * the only file outside the registry that names a concrete module type. */
#include "FBModuleRegistry.h"
#include "FBStore.h"
#include "FBStoreModule.h"

namespace FlightBox {

void FBRegisterStoreModules() {
  for (const FBStoreSpec *spec : kStoreCatalogue)
    FBModuleRegistry::Register(spec->Key, [spec]() -> std::unique_ptr<FBModule> {
      return std::make_unique<FBStoreModule>(*spec);
    });
}

} // namespace FlightBox
