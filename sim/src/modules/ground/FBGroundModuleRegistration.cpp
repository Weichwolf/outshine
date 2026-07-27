/* The ground-target catalogue's registration with FBModuleRegistry — one name per target class, all of
 * them the same class (modules/ground/FBGroundModule). Its own translation unit for the same reason the
 * F-16's and the stores' are: this is the only file outside the registry that names a concrete module
 * type. */
#include "FBGroundModule.h"
#include "FBGroundTarget.h"
#include "FBModuleRegistry.h"

namespace FlightBox {

void FBRegisterGroundModules() {
  for (const FBGroundTargetSpec *spec : kGroundTargetCatalogue) {
    FBModuleRegistry::Register(spec->Key, [spec]() -> std::unique_ptr<FBModule> {
      return std::make_unique<FBGroundModule>(*spec);
    });
  }
}

} // namespace FlightBox
