/* The live autopilot state, shared by every layer that reads it (bus, HUD, telemetry, guidance). */
#ifndef MODE_H
#define MODE_H

namespace outshine {

enum class Mode { Manual, Direct, Course };

} // namespace outshine
#endif
