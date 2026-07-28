/* One name per catalogue row, all the same class — the registration file walks kAircraftCatalogue
 * exactly as FBSiteModuleRegistration walks kSiteCatalogue and FBStoreModuleRegistration walks
 * kStoreCatalogue. The only file outside the registry that names FBAirModule. */
#include "FBAircraft.h"
#include "FBAirModule.h"
#include "FBModuleRegistry.h"

namespace FlightBox::Modules {

void FBRegisterAirModules() {
  for (const FBAircraftSpec *spec : kAircraftCatalogue) {
    FBModuleRegistry::Register(spec->Key, [spec]() -> std::unique_ptr<FBModule> {
      return std::make_unique<FBAirModule>(*spec);
    });
  }
}

} // namespace FlightBox::Modules
