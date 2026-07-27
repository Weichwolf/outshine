/* The live autopilot state, shared by every layer that reads it (bus, HUD, telemetry, guidance). */
#ifndef FBMODE_H
#define FBMODE_H

namespace FlightBox {

enum class FBMode { Manual, Direct, Course };

} // namespace FlightBox
#endif
