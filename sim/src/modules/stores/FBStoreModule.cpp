#include "FBStoreModule.h"

namespace FlightBox::Modules {

void FBStoreModule::Run(Fdm::fb_fdm_state &st, double dt, const Units::FBUnitRegistry *units, const World::FBWorld *world) {
  (void)units; (void)world;
  if (!Fdm_) return;
  AccS_ += dt;
  LastSub_ = 0;
  for (int k = 0; AccS_ >= Fdm::FBFdm::kStepS && k < 12; k++) {
    Fdm_->Step(st);
    AccS_ -= Fdm::FBFdm::kStepS;
    LastSub_++;
  }
}

} // namespace FlightBox::Modules
