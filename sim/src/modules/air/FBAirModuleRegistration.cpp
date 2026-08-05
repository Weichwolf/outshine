/* One name per catalogue row, all the same class — and the ONLY place a whole catalogue is ever
 * visible. A factory hands its module ONE row, so a catalogue aircraft cannot learn what else the
 * scenario contains (core/FBAircraftCatalogue.h). The only file outside the registry that names
 * FBAirModule. */
#include <vector>

#include "FBAirModule.h"
#include "FBModuleRegistry.h"

namespace FlightBox::Modules {

namespace {
/* The rows BACK the factories installed below, so they must outlive them: a factory in a
 * process-lifetime registry cannot borrow a caller's local. APPENDED and never replaced — a second
 * registration overwrites the map entries, but a module created from the first still points into the
 * first catalogue, and freeing it would be a dangling reference nobody can see. */
std::vector<std::unique_ptr<FBAircraftCatalogue>> &Held() {
  static std::vector<std::unique_ptr<FBAircraftCatalogue>> v;
  return v;
}
} // namespace

void FBRegisterAirModules(FBAircraftCatalogue catalogue) {
  Held().push_back(std::make_unique<FBAircraftCatalogue>(std::move(catalogue)));
  const FBAircraftCatalogue &held = *Held().back();
  for (size_t i = 0; i < held.Size(); i++) {
    const FBAircraftSpec *spec = &held.At(i);
    FBModuleRegistry::Register(spec->Key, [spec]() -> std::unique_ptr<FBModule> {
      return std::make_unique<FBAirModule>(*spec);
    });
  }
}

} // namespace FlightBox::Modules
