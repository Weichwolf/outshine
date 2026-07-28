/* The ONE file allowed to name FBMig29Module for registration; every caller of
 * FBRegisterBuiltinModules() sees only the neutral declaration. */
#include "FBModuleRegistry.h"
#include "FBMig29Module.h"

namespace FlightBox::Modules {

void FBRegisterMig29Module() {
  FBModuleRegistry::Register("mig29", [] { return std::make_unique<FBMig29Module>(); });
}

} // namespace FlightBox::Modules
