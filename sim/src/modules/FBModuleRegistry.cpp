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
  return it == Registry().end() ? nullptr : it->second();
}

void FBRegisterBuiltinModules() {
  FBRegisterF16Module();
  FBRegisterMig29Module();
  FBRegisterStoreModules();
  FBRegisterMissileModules();
  FBRegisterGroundModules();
}

} // namespace FlightBox::Modules
