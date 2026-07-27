/* The module-wide master mode. AUTHORITY lives on the module, not globally; the enum is shared because
 * Input, Displays and Weapons all take it as a parameter. */
#ifndef FBMASTERMODE_H
#define FBMASTERMODE_H

namespace FlightBox {

enum class FBMasterMode { Nav, AirToAir, AirToGround, Dogfight };

} // namespace FlightBox
#endif
