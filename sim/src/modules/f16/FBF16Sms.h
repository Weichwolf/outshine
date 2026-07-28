/* FlightBox — FBF16Sms: WHERE the nine pylons sit on this airframe, and therefore what a store does to
 * its balance — all behaviour is systems/FBStoresSystem. Every number is anchored to a reference the
 * pinned f16.xml itself provides; the longitudinal station is the CG station for all nine, the minimal
 * assumption, because no per-station fuselage station is citable. Derivation:
 * doc/modules-f16.md §9.1. */
#ifndef FBF16SMS_H
#define FBF16SMS_H

#include "FBStoresSystem.h"

namespace FlightBox::Modules {

class FBF16Sms : public Weapons::FBStoresSystem {
public:
  FBF16Sms();
};

} // namespace FlightBox::Modules
#endif
