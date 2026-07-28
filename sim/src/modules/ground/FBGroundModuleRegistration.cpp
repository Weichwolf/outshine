/* One name per target class, all the same class; the only file outside the registry that names it. */
#include "FBGroundModule.h"
#include "FBGroundTarget.h"
#include "FBModuleRegistry.h"

namespace FlightBox::Modules {

void FBRegisterGroundModules() {
  for (const FBGroundTargetSpec *spec : kGroundTargetCatalogue) {
    FBModuleRegistry::Register(spec->Key, [spec]() -> std::unique_ptr<FBModule> {
      return std::make_unique<FBGroundModule>(*spec);
    });
  }
}

} // namespace FlightBox::Modules
