/* FlightBox — FBModuleRegistry: name -> factory for the modules a mission's `module <name>` line can
 * select. Declared here so a caller that only wants to CREATE a module never includes a concrete
 * module's header. */
#ifndef FBMODULEREGISTRY_H
#define FBMODULEREGISTRY_H
#include <functional>
#include <memory>
#include <string>
#include "FBModule.h"

namespace FlightBox::Modules {

using FBModuleFactory = std::function<std::unique_ptr<FBModule>()>;

class FBModuleRegistry {
public:
  static void Register(const std::string &name, FBModuleFactory factory);
  static std::unique_ptr<FBModule> Create(const std::string &name);
};

/* One entry point per module FAMILY, each defined in that family's own *Registration.cpp — the only
 * files allowed to name a concrete module type. */
void FBRegisterF16Module();
void FBRegisterStoreModules();
void FBRegisterMissileModules();
void FBRegisterGroundModules();

/* Every module this link target was built with. Idempotent; call once before the first Create(). */
void FBRegisterBuiltinModules();

} // namespace FlightBox::Modules
#endif
