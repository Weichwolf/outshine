/* FlightBox — FBModuleRegistry: name -> factory registry for the modules a mission can select via its
 * `module <name>` line (doc/mission-format.md). A headless runner (FBMissionRunner.h) resolves a
 * module by NAME only and holds everything behind FBModule* from then on — it never names a concrete
 * module type, so it stays airframe-agnostic as more modules (Ka-52, F-18, ...) register. Each module
 * registers itself from its OWN directory (modules/f16/FBF16ModuleRegistration.cpp) behind
 * FBRegisterBuiltinModules(), declared here so a caller that only wants to CREATE a module never has
 * to include a concrete module's header. */
#ifndef FBMODULEREGISTRY_H
#define FBMODULEREGISTRY_H
#include <functional>
#include <memory>
#include <string>
#include "FBModule.h"

namespace FlightBox {

using FBModuleFactory = std::function<std::unique_ptr<FBModule>()>;

class FBModuleRegistry {
public:
  static void Register(const std::string &name, FBModuleFactory factory);
  static std::unique_ptr<FBModule> Create(const std::string &name);
};

/* One entry point per module FAMILY, each defined in that family's OWN directory — the only files
 * allowed to name a concrete module type (modules/f16/FBF16ModuleRegistration.cpp,
 * modules/stores/FBStoreModuleRegistration.cpp, modules/missile/FBMissileModuleRegistration.cpp — the
 * catalogue's Guided flag decides which of the last two claims an entry, read in one place each). */
void FBRegisterF16Module();
void FBRegisterStoreModules();
void FBRegisterMissileModules();

/* Registers every module this link target was built with: the flyable aircraft AND the released
 * stores, which are modules in exactly the same sense (they are their own FDM + FBModule, spawned
 * through the same registry by name). Idempotent (re-registering just overwrites the map entry) —
 * safe to call once per run before the first FBModuleRegistry::Create, from any App main() or
 * FBMissionRunner itself. */
void FBRegisterBuiltinModules();

} // namespace FlightBox
#endif
