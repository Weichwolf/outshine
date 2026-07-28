/* The ONE file allowed to name FBF16Module for registration; every caller of
 * FBRegisterBuiltinModules() sees only the neutral declaration. */
#include "FBModuleRegistry.h"
#include "FBF16Module.h"

namespace FlightBox::Modules {

void FBRegisterF16Module() {
  FBModuleRegistry::Register("f16", [] { return std::make_unique<FBF16Module>(); });
}

} // namespace FlightBox::Modules
