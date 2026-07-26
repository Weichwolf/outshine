#include "FBModuleRegistry.h"
#include <map>

namespace FlightBox {

namespace {
/* Function-local static (Meyer's singleton), not a namespace-scope global: populated explicitly by
 * FBRegisterBuiltinModules() at a known point in main(), never by static-initialization order across
 * translation units — avoids both the SIOF and the "unreferenced .o in a static archive never links"
 * trap a self-registering namespace-scope static would hit here. */
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
  FBRegisterStoreModules();
  FBRegisterMissileModules();
}

} // namespace FlightBox
