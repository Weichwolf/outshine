/* FlightBox — FBMode: the live autopilot state, shared by every layer that reads it (FBState/HUD,
 * telemetry, the guidance systems themselves). Lives in core/ so core types never have to reach into
 * systems/ just for this enum. */
#ifndef FBMODE_H
#define FBMODE_H

namespace FlightBox {

enum class FBMode { Manual, Loiter, LowLevel, Direct };

} // namespace FlightBox
#endif
