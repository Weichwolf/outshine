/* FlightBox — FBRwrSystem: the passive half of the Defensive slot, and the exact mirror image of
 * systems/FBRadarSystem. The radar asks "what is out there"; this asks "who is looking at me", and it
 * answers that question the only honest way — by listening to what other units are RADIATING
 * (units/FBUnit.h's FBEmitterSignature) and checking, geometrically, whether that beam can reach this
 * aircraft and whether this aircraft's antennas can hear it.
 *
 * WHAT IT IS NOT, and the source says so in as many words (doc/f16/defence-rwr-cm.md §2.1): "the RWR is
 * explicitly NOT omniscient — it only reports detected emissions, never whether the threat radar can
 * actually see ownship, nor whether it's tracking ownship vs. another aircraft on the same bearing. A
 * faithful simulation must model this as a geometry-gated emission detector, not a ground-truth threat
 * oracle." Every consequence of that sentence is implemented literally:
 *   - AN EMITTER THAT IS NOT POINTED AT YOU IS NOT HEARD. The beam window travels in the emitter's
 *     signature, body-referenced; this class rotates itself into the emitter's frame with the same
 *     transform the emitter's own antenna uses (core/FBGeodesy.h) and tests whether it is inside it. A
 *     radar sweeping a search volume therefore paints everything in that volume; a radar in single-
 *     target track paints one aircraft and NOBODY else, which is why a tracking radar's warning is a
 *     personal one.
 *   - A BEAM YOUR ANTENNAS CANNOT SEE IS NOT HEARD EITHER. The receiver has its own coverage
 *     (ElevCoverageDeg below): 360 degrees in azimuth, but a limited elevation window, so a threat far
 *     above or below the fuselage plane sits in a genuine BLIND ZONE and its warning silently
 *     disappears — including a lock or a launch warning you already had. §2.1 names this exactly, and
 *     it is a property of the RECEIVER's attitude, so a hard manoeuvre can create it.
 *   - RANGE IS NOT MEASURED. Nothing published carries metres (core/FBRwrThreat.h). What this class
 *     computes is RECEIVED POWER, and how far a beam is heard follows from the one-way path it takes to
 *     get here against the two-way path the emitter needs for its own echo (kBeamRangeFactor).
 *   - IT CANNOT NAME THE EMITTER. Kind is an ESTIMATE. Today's threat library has one entry per emitter
 *     class and is therefore always right; the field is a receiver's opinion by construction, so the day
 *     it is wrong nothing downstream changes shape.
 *
 * WHERE THE REGISTRY MAY GO (CLAUDE.md "Kein Cheaten"): this class joins systems/FBRadarSystem and
 * systems/FBDatalinkSystem as the THIRD and last thing under systems/ or modules/ that holds a
 * units/FBUnitRegistry, and it holds it for the narrowest possible reason — a signature is exactly what
 * a receiver may observe. Everything downstream reads threats out of FBState like any other instrument.
 *
 * INTERFACE + GENERIC DEFAULT in one class, the FBRadarSystem/FBDatalinkSystem pattern: Run() is the
 * override point, the receiver's numbers are protected hooks, and modules/f16/FBF16Rwr supplies the
 * AN/ALR-56M's. */
#ifndef FBRWRSYSTEM_H
#define FBRWRSYSTEM_H

#include "FBRwrThreat.h"
#include "FBState.h"
#include "FBTeam.h"
#include "FBTelemetry.h"
#include "FBUnits.h"
#include "FBFdm.h"

namespace FlightBox {

class FBUnit;
class FBUnitRegistry;

class FBRwrSystem : public FBTelemetrySource {
public:
  /* HOW MUCH FURTHER A BEAM IS HEARD THAN IT REACHES [SET, derived]. The emitter needs its pulse to fly
   * out AND come back, so the echo it processes falls off as 1/r^4; this receiver only sits in the
   * outbound half, so what it hears falls off as 1/r^2. For the same receiver sensitivity that puts the
   * warning range at roughly sqrt(sigma * R^4 / R_rwr^2)-ish — i.e. a MULTIPLE of the emitter's own
   * detection range, not a fraction. Public literature puts the ratio anywhere from 1.5 to 3 depending
   * on the receiver; 2.0 is the middle of that and has the property the mission can measure: you get a
   * warning before he gets a track. */
  static constexpr double kBeamRangeFactor = 2.0;
  /* How long a threat is HELD after its emission stops being heard [SET]. A beam that sweeps away and a
   * transmitter that switches off look identical for the first instant, so the symbol is not blinked out
   * on the first missed look — the same "hold it briefly rather than lose it" rule
   * FBRadarSystem::kCoastFrames and FBDatalinkSystem::kDropAfterCycles apply, in seconds because this
   * receiver has no frame of its own (it listens continuously). */
  static constexpr double kHoldS = 2.0;
  /* The new-threat window: how long a freshly detected emitter reports as NEW [SET]. The real box plays
   * a tone on detection (doc/f16/defence-rwr-cm.md §2.1); with no audio, "new" is a published state with
   * a lifetime instead, long enough for a 10 Hz consumer to see it and short enough not to blur into the
   * steady picture. */
  static constexpr double kNewThreatS = 1.0;
  /* The scope's radial scale (core/FBRwrThreat.h: the symbol's distance from the centre is LETHALITY,
   * not range). Mode decides the ring — search outside, track just inside it, missile guidance innermost
   * (§2.1's table, verbatim) — and received power moves the symbol within its ring. [SET] */
  static constexpr double kLethalitySearch = 0.20;
  static constexpr double kLethalityTrack = 0.55;
  static constexpr double kLethalityMissile = 0.85;
  static constexpr double kLethalitySignalWeight = 0.15;

  ~FBRwrSystem() override = default;

  /* Boot wiring, once per unit (FBModule::SetUnitIdentity): who this aircraft is, so the receiver does
   * not warn about its own radar. The TEAM is taken and deliberately NEVER read: a warning receiver
   * hears a waveform, not an allegiance, so a friendly radar tracking you produces exactly the same
   * symbol as a hostile one. It is stored because every identity-taking system takes the same pair, and
   * the day a threat library classifies by emitter type it will still not classify by faction. */
  void SetIdentity(int selfId, FBUnitTeam team) { SelfId_ = selfId; SelfTeam_ = team; }

  void SetPowered(bool on);
  bool Powered() const { return Powered_; }

  /* The display filter for search-class emitters (doc/f16/defence-rwr-cm.md §2.1's SEARCH button): with
   * it off, surveillance/search symbols are hidden — and the fact that something IS being hidden stays
   * visible (FBRwrBlock::HiddenSearch), because suppression must be distinguishable from absence. */
  void SetSearchShown(bool on) { SearchShown_ = on; }
  bool SearchShown() const { return SearchShown_; }

  /* The override point (class banner). `st` is this aircraft's own pose/attitude, `net` the borrowed
   * registry of published unit snapshots (null = nothing to hear), `simTimeS` the module's own absolute
   * sim clock. Writes the RWR block and nothing else. */
  virtual void Run(FBState &state, const fb_fdm_state &st, const FBUnitRegistry *net, double simTimeS);

  const char *TelemetryName() const override { return "rwr"; }
  void DeclareTelemetry(FBTelemetrySchema &schema) const override;
  void SampleTelemetry(FBTelemetryRow &row) const override;

protected:
  /* ---- the receiver's own numbers: the hooks a real box overrides (modules/f16/FBF16Rwr) ----
   * Azimuth is not among them because every warning receiver worth the name covers 360 degrees; what
   * differs between boxes is how far above and below the fuselage plane they can hear, and how many
   * symbols the scope shows at once. */
  virtual double ElevCoverageDeg() const { return 60.0; }   /* generic 4-quadrant receiver */
  virtual int    MaxDisplayed() const { return kMaxRwrThreats; }

  /* What the receiver makes of a signal it heard. The default library is one entry per emitter class;
   * a module with a real threat library overrides it. */
  virtual FBEmitterKind Classify(const FBEmitterSignature &sig) const { return sig.Kind; }

private:
  struct Threat {
    int    Id = 0;
    int    UnitId = 0;          /* the detection-to-detection correlation key; NEVER published */
    double BearingDeg = 0.0, ElDeg = 0.0;
    double SignalNorm = 0.0;
    double FirstS = 0.0, LastS = 0.0;
    FBRwrThreatMode Mode = FBRwrThreatMode::Search;
    FBEmitterKind Kind = FBEmitterKind::Unknown;
    bool   Heard = false;       /* this tick */
    bool   Blind = false;       /* the beam is arriving and the antennas cannot hear it (log once) */
  };

  /* Is this aircraft inside that emitter's beam? Pure geometry on published data: the emitter's beam
   * window, the emitter's ATTITUDE (out of its published pose — the beam is body-referenced) and the
   * line of sight from it to here. */
  static bool BeamCovers(const FBEmitterSignature &sig, double rollDeg, double pitchDeg, double yawDeg,
                         double eastM, double northM, double upM);
  static FBRwrThreatMode ModeOf(const FBEmitterSignature &sig, FBEmitterKind kind);
  double Lethality(FBRwrThreatMode mode, double signalNorm) const;
  void   Publish(FBState &state, double simTimeS);

  Threat Threats_[kMaxRwrThreats]{};
  int    Count_ = 0;
  int    NextId_ = 1;

  int SelfId_ = 0;
  FBUnitTeam SelfTeam_ = FBUnitTeam::Friendly;
  bool Powered_ = true;
  bool SearchShown_ = true;

  /* Telemetry's view of the last Run. */
  int   ThreatCount_ = 0, PriorityMode_ = 0;
  float TopBearingDeg_ = 0.0f, TopElDeg_ = 0.0f, TopLethality_ = 0.0f;
  bool  Launch_ = false, Activity_ = false, NewThreat_ = false;
  int   BlockStatus_ = 0;
};

} // namespace FlightBox
#endif
