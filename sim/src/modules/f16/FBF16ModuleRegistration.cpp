/* Registers "f16" with FBModuleRegistry — the ONE file in the F-16's own directory allowed to name
 * FBF16Module directly for this purpose; every caller of FBRegisterBuiltinModules() only ever sees the
 * neutral declaration in modules/FBModuleRegistry.h. */
#include "FBModuleRegistry.h"
#include "FBF16Module.h"

namespace FlightBox {

void FBRegisterBuiltinModules() {
  FBModuleRegistry::Register("f16", [] { return std::make_unique<FBF16Module>(); });
}

} // namespace FlightBox
