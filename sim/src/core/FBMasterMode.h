/* FlightBox — FBMasterMode: the module-wide master mode (NAV/A-A/A-G/DGFT). Authority lives on the
 * module (FBModule-derived), not globally: it gates which display pages are shown, how HOTAS events
 * are routed, and which weapon (if any) is selected. Shared here (core/) because Input, Displays and
 * Weapons systems all take it as a parameter — a common enum, not a dependency on any one system. */
#ifndef FBMASTERMODE_H
#define FBMASTERMODE_H

namespace FlightBox {

enum class FBMasterMode { Nav, AirToAir, AirToGround, Dogfight };

} // namespace FlightBox
#endif
