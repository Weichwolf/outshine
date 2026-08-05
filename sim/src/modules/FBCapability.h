/* FlightBox — the module capability list: ONE table, three readers (C++ accessors, the control loop,
 * an LLM tool schema). Deliberately include-free so a schema generator can read the list without
 * pulling the systems tree in; the types are named only where the macro is expanded.
 * doc/architecture.md §The layering pattern, doc/mods.md §2.1. */
#ifndef FBCAPABILITY_H
#define FBCAPABILITY_H

#include <cstdint>

namespace FlightBox::Modules {

/* Accessor, C++ type, WIRE NAME. The wire name is the key a mod file and a tool schema spell; it is
 * not derived from the accessor because the two answer different questions (`PilotSystem` is a C++
 * name that must not collide with the FBPilot class, `pilot` is what the thing IS). */
#define FB_MODULE_CAPABILITIES(X)                                             \
  X(Autopilot,       Systems::FBAutopilot,             "autopilot")           \
  X(FlightControl,   Systems::FBFlightControl,         "flight_control")      \
  X(PilotSystem,     Pilot::FBPilot,                   "pilot")               \
  X(Controls,        Systems::FBAirframeControls,      "airframe_controls")   \
  X(HumanInput,      Systems::FBInputSystem,           "human_input")         \
  X(Displays,        Systems::FBDisplaySystem,         "displays")            \
  X(AirDataSystem,   Systems::FBAirDataSystem,         "air_data")            \
  X(NavSystem,       Systems::FBNavSystem,             "navigation")          \
  X(WarningSystem,   Systems::FBWarningSystem,         "warning")             \
  X(RadarAltimeter,  Systems::FBRadarAltimeter,        "radar_altimeter")     \
  X(Commands,        FBCommandBus,                     "command_bus")         \
  X(Datalink,        Sensors::FBDatalinkSystem,        "datalink")            \
  X(Radar,           Sensors::FBRadarSystem,           "radar")               \
  X(Rwr,             Sensors::FBRwrSystem,             "rwr")                 \
  X(Irst,            Sensors::FBIrstSystem,            "irst")                \
  X(Visual,          Sensors::FBVisualSystem,          "visual")              \
  X(Countermeasures, Sensors::FBCountermeasureSystem,  "countermeasures")     \
  X(Stores,          Weapons::FBStoresSystem,          "stores")              \
  X(Guns,            Weapons::FBGunSystem,             "guns")                \
  X(FlightPlan,      FBFlightPlan,                     "flight_plan")

enum class FBCapability : std::uint8_t {
#define FB_CAP_ENUM(Acc, Type, Wire) Acc,
  FB_MODULE_CAPABILITIES(FB_CAP_ENUM)
#undef FB_CAP_ENUM
};

inline constexpr int kFBCapabilityCount = 0
#define FB_CAP_COUNT(Acc, Type, Wire) + 1
    FB_MODULE_CAPABILITIES(FB_CAP_COUNT)
#undef FB_CAP_COUNT
    ;

using FBCapabilityMask = std::uint32_t;
static_assert(kFBCapabilityCount <= 32, "FBCapabilityMask is the declaration set — widen it");

constexpr FBCapabilityMask FBCapabilityBit(FBCapability c) {
  return (FBCapabilityMask)1u << (unsigned)c;
}

/* THE MACHINE-READABLE HALF: name and type survive to run time, so the same list that binds the
 * control loop can be emitted as a tool schema without a second, drifting copy of it. */
struct FBCapabilityDesc {
  FBCapability Id;
  const char *Name;
  const char *Type;
};

inline constexpr FBCapabilityDesc kFBCapabilityTable[kFBCapabilityCount] = {
#define FB_CAP_ROW(Acc, Type, Wire) {FBCapability::Acc, Wire, #Type},
    FB_MODULE_CAPABILITIES(FB_CAP_ROW)
#undef FB_CAP_ROW
};

} // namespace FlightBox::Modules
#endif
