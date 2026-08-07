/* What a RADAR puts into the air, as the thing another aircraft's receiver may legitimately notice —
 * the first emission in UnitSignature that has a DIRECTION, which is why this file exists: a beam
 * decides WHO hears it, and "radar on/off" would model an omniscient warning receiver.
 * The window is BODY-REFERENCED to the emitter, so a receiver rotates itself into that frame with the
 * same transform the antenna uses and asks the one question: am I inside the beam?
 * NO IDENTITY, NO POWER RATING, NO FREQUENCY — the same austerity RadarContact.h keeps on the other
 * side of the fence. doc/core.md, Abschnitt 8.3. */
#ifndef EMITTER_H
#define EMITTER_H

#include <cstdint>

namespace outshine {

/* Telemetry-visible ordinals — append, never reorder. */
enum class EmitterMode : uint8_t { None = 0, Search, Track, Guidance };

/* What is radiating, as the emitter itself knows it. A receiver only ever ESTIMATES this — it hears a
 * waveform, not a nameplate — hence RwrSystem's own estimated copy. The two SURFACE values name what
 * sits BEHIND the antenna: a telephone, or a launcher. */
enum class EmitterKind : uint8_t { Unknown = 0, AirborneFireControl, MissileSeeker,
                                     SurfaceEarlyWarning, SurfaceFireControl };

/* HOW MANY ANTENNAS one unit may radiate from at once. Two, because an air-defence position is two
 * sets by construction — the acquisition radar keeps sweeping while the fire control stares — and
 * collapsing them into one would delete the search->track transition for every observer except the one
 * being tracked. An airframe carries one antenna and writes index 0 only, so its signature is the
 * scalar it always was. doc/modules/ground/module.md §Spec 6. */
constexpr int kMaxEmitterBeams = 2;

inline const char *EmitterModeStr(EmitterMode m) {
  switch (m) {
    case EmitterMode::None: return "none";
    case EmitterMode::Search: return "search";
    case EmitterMode::Track: return "track";
    case EmitterMode::Guidance: return "guidance";
  }
  return "?";
}

inline const char *EmitterKindStr(EmitterKind k) {
  switch (k) {
    case EmitterKind::Unknown: return "unknown";
    case EmitterKind::AirborneFireControl: return "fire-control";
    case EmitterKind::MissileSeeker: return "missile-seeker";
    case EmitterKind::SurfaceEarlyWarning: return "surface-search";
    case EmitterKind::SurfaceFireControl: return "surface-fire-control";
  }
  return "?";
}

struct EmitterSignature {
  EmitterMode Mode = EmitterMode::None;   /* None = this unit is not radiating at all */
  EmitterKind Kind = EmitterKind::Unknown;
  /* Centre + half-width, azimuth off the nose and elevation off the boresight plane. Search = the whole
   * swept volume; Track/Guidance = a pencil cone on the tracked target. */
  float AzCenterDeg = 0.0f, AzHalfDeg = 0.0f;
  float ElCenterDeg = 0.0f, ElHalfDeg = 0.0f;
  float RangeM = 0.0f;   /* the emitter's own detection gate — the stand-in for transmitted power */
};

} // namespace outshine
#endif
