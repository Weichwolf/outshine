#include "FBModuleRegistry.h"
#include <map>

namespace FlightBox::Modules {

namespace {
/* Function-local static, populated explicitly at a known point: avoids both the static-init order
 * fiasco and the "unreferenced .o in a static archive never links" trap of self-registration. */
std::map<std::string, FBModuleFactory> &Registry() {
  static std::map<std::string, FBModuleFactory> r;
  return r;
}
} // namespace

void FBModuleRegistry::Register(const std::string &name, FBModuleFactory factory) {
  Registry()[name] = std::move(factory);
}

std::unique_ptr<FBModule> FBModuleRegistry::Create(const std::string &name) {
  auto it = Registry().find(name);
  if (it == Registry().end()) return nullptr;
  std::unique_ptr<FBModule> m = it->second();
  /* The key is stamped HERE, from the map key the caller asked for — a module that spelled its own
   * name would be a second truth about the same string, and an eye reports that string verbatim. */
  if (m) m->SetTypeName(name);
  return m;
}

void FBRegisterBuiltinModules() {
  FBRegisterF16Module();
  FBRegisterMig29Module();
  FBRegisterStoreModules();
  FBRegisterMissileModules();
  FBRegisterGroundModules();
  FBRegisterAirModules();
}

} // namespace FlightBox::Modules
