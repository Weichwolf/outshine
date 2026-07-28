/* One name per row, all the same class — twice over: the inert targets and the air-defence positions.
 * The only file outside the registry that names either. */
#include "FBGroundModule.h"
#include "FBGroundTarget.h"
#include "FBModuleRegistry.h"
#include "FBSite.h"
#include "FBSiteModule.h"

namespace FlightBox::Modules {

void FBRegisterGroundModules() {
  for (const FBGroundTargetSpec *spec : kGroundTargetCatalogue) {
    FBModuleRegistry::Register(spec->Key, [spec]() -> std::unique_ptr<FBModule> {
      return std::make_unique<FBGroundModule>(*spec);
    });
  }
  for (const FBSiteSpec *spec : kSiteCatalogue) {
    FBModuleRegistry::Register(spec->Key, [spec]() -> std::unique_ptr<FBModule> {
      return std::make_unique<FBSiteModule>(*spec);
    });
  }
}

} // namespace FlightBox::Modules
