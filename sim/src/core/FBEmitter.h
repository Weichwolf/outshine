/* FlightBox — FBEmitterSignature: what a RADAR puts into the air, as the thing another aircraft's
 * receiver can legitimately notice. It is the third emission to join units/FBUnit's FBUnitSignature
 * (after the datalink transmitter and the IFF transponder) and the first one that has a DIRECTION,
 * which is the whole reason this file exists.
 *
 * A RADAR DOES NOT RADIATE IN ALL DIRECTIONS. It puts its energy into a beam, and where that beam is
 * pointed decides who hears it — so a signature that were only "radar on/off" would model an omniscient
 * warning receiver, which is exactly what doc/f16/defence-rwr-cm.md §2.1 says the real box is not
 * ("a geometry-gated emission detector, not a ground-truth threat oracle"). What travels here is
 * therefore the beam's own geometry, BODY-REFERENCED to the emitting aircraft: the receiver already has
 * that aircraft's published pose (FBUnit::GetPose), so it can rotate itself into the emitter's frame
 * with the SAME transform the emitter's own antenna uses (core/FBGeodesy.h's FBEnuToBodyLos) and ask
 * the one question that matters — am I inside that beam?
 *
 * THE THREE SIGNALS, and why they are not one. A radar SEARCHING, a radar TRACKING and a missile seeker
 * are three different things to be hit by, and the difference is tactical, not cosmetic:
 *   Search    the antenna sweeps a VOLUME, so the beam crosses everything inside that volume once per
 *             frame. The published window is therefore the whole scan volume — being "in the beam" of a
 *             searching radar means being inside the volume it is sweeping. Information, not a threat:
 *             somebody is looking, nobody has found you.
 *   Track     single-target track collapses the pattern onto ONE target: all power, a pencil beam,
 *             continuously. The published window is a narrow cone pointed at what the set is tracking,
 *             so only that target hears it. A warning: he has you.
 *   Guidance  a tracking radar that is also supporting a missile in flight (the launcher's midcourse
 *             uplink, core/FBWeaponUplink.h). Same beam, different meaning — doc/f16/defence-rwr-cm.md
 *             §1's flashing circle, i.e. the MISSILE LAUNCH light.
 * A missile's own seeker is not a fourth MODE but a different KIND of emitter (FBEmitterKind), because
 * what makes it a threat is what is behind the antenna, not how it is scanning.
 *
 * NO IDENTITY, NO POWER RATING, NO FREQUENCY. This is deliberately the same austerity core/
 * FBRadarContact.h keeps on the other side of the fence: the signature says what is being radiated and
 * from where, never who is doing it — the receiver classifies by what it hears (systems/FBRwrSystem),
 * and it can be wrong. RangeM is the emitter's own detection gate: the ONE number that stands in for
 * transmitted power here, because a set that can detect a fighter at 40 nm is putting out roughly ten
 * times the energy of one gated to 10 nm. How far that is HEARD is the receiver's business (a one-way
 * path against the emitter's own two-way one — systems/FBRwrSystem::kBeamRangeFactor), not the
 * emitter's. */
#ifndef FBEMITTER_H
#define FBEMITTER_H

#include <cstdint>

namespace FlightBox {

/* Telemetry-visible ordinals — append, never reorder. */
enum class FBEmitterMode : uint8_t { None = 0, Search, Track, Guidance };

/* WHAT is radiating, as the emitter itself knows it. A receiver only ever ESTIMATES this (it hears a
 * waveform, not a nameplate), which is why systems/FBRwrSystem carries its own estimated copy rather
 * than reading this field straight through. */
enum class FBEmitterKind : uint8_t { Unknown = 0, AirborneFireControl, MissileSeeker };

inline const char *FBEmitterModeStr(FBEmitterMode m) {
  switch (m) {
    case FBEmitterMode::None: return "none";
    case FBEmitterMode::Search: return "search";
    case FBEmitterMode::Track: return "track";
    case FBEmitterMode::Guidance: return "guidance";
  }
  return "?";
}

inline const char *FBEmitterKindStr(FBEmitterKind k) {
  switch (k) {
    case FBEmitterKind::Unknown: return "unknown";
    case FBEmitterKind::AirborneFireControl: return "fire-control";
    case FBEmitterKind::MissileSeeker: return "missile-seeker";
  }
  return "?";
}

struct FBEmitterSignature {
  FBEmitterMode Mode = FBEmitterMode::None;   /* None = this unit is not radiating at all */
  FBEmitterKind Kind = FBEmitterKind::Unknown;
  /* The beam window, BODY-REFERENCED to the emitting aircraft (see the banner): centre + half-width in
   * azimuth off the nose and elevation off the boresight plane. In Search this is the swept volume; in
   * Track/Guidance a pencil cone pointed at the tracked target. */
  float AzCenterDeg = 0.0f, AzHalfDeg = 0.0f;
  float ElCenterDeg = 0.0f, ElHalfDeg = 0.0f;
  float RangeM = 0.0f;   /* the emitter's own detection gate — the stand-in for transmitted power */
};

} // namespace FlightBox
#endif
