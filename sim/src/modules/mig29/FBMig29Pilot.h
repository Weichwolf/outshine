/* FlightBox — FBMig29Pilot: this jet's pilot NUMBERS. It overrides pilot/FBPilot's virtual hooks and
 * nothing else; the phase machine itself is generic, and that IS the stage-2a proof — a second airframe
 * flies the same Preflight->Takeoff->Climb->Route->Approach->Flare->Rollout procedure by declaring
 * different numbers, not different code.
 *
 * Provenance of every value below, per CLAUDE.md ("jede Zahl trägt ihre Herkunft"):
 *   [DOC]  stated in doc/mig29/procedures.md's quick-reference table, with its DCS-EA page
 *   [MESS] measured on the flown model by `make -C sim test-mig29` (anchor id given)
 *   [ABL]  derived, with the derivation written out here
 *   [SET]  a declared FlightBox setting, with its consequence named
 * The MiG-29's approach schedule is deliberately NOT a re-tuned F-16 one — doc/mig29/procedures.md §1
 * says so explicitly, and the numbers below are why: a faster final, a lower flare, a nozzle-strike
 * geometry 1.5 deg tighter than the flight monitor's contact-pitch limit, and a drag chute. */
#ifndef FBMIG29PILOT_H
#define FBMIG29PILOT_H

#include "FBPilot.h"
#include "FBMig29Radar.h"

namespace FlightBox::Modules {

class FBMig29Pilot : public Pilot::FBPilot {
public:
  /* ---- GCI, and it is the whole doctrinal difference to the F-16's datalink.
   *
   * The F-16 is a peer node in a network that hands it a correlated track picture. This aircraft is
   * flown to the merge BY SOMEBODY ELSE, over VOICE, and the documented path is
   * "controller -> pilot -> MANUAL DATA ENTRY -> radar scan solution"
   * (doc/modules/mig29/datalink-gci.md §2.2, which calls it the mechanism to implement first). So a GCI
   * call is not knowledge and it is not a track: it is FOUR NUMBERS the pilot has to TYPE, one entry
   * per decision tick, in the DED latency class, rejectable by the bus like anything else. Between the
   * call and the antenna moving there are real seconds.
   *
   * WHAT THE CALL IS NOT: it never becomes a contact, it is never correlated with what the radar then
   * finds, and it does not age into a track file. The controller says where to look; finding anything
   * is still the pilot's problem, with his own sensor and its own detection time.
   *
   * The BEARING half of the BRAA is flown as the briefed vector — i.e. as the mission's own waypoint,
   * the same one the generic intercept Search phase already steers to — and reaches the radar as the
   * discrete ZONE. The RANGE and ALTITUDE halves are the documented range-angle entry: the elevation
   * the antenna bar is aimed at follows from expected range and expected RELATIVE altitude. */
  static constexpr int kMaxGciCalls = 8;
  bool BriefGci(double atS, double brgDeg, double rangeKm, double altKm);

  /* The generic phase machine, plus one thing before it: whatever the controller said is due. */
  Pilot::FBPilotCommands Run(const FBState &state, FBCommandBus &avionics,
                             const Systems::FBAirframeControls &airframe, const Fdm::fb_fdm_state &st,
                             const FBFlightPlan &plan, const FBRunway *runway, double dt) override;

protected:
  /* The non-locking search mode of the N019 — RAD. The generic BVR phase can only ASK for "the mode
   * that finds everything and locks nothing"; which ordinal that is, only this module knows. */
  int SearchRadarModeOrdinal() const override { return (int)FBMig29RadarMode::Rad; }
  /* [ABL] Der Nahkampf-Modus, den die BFM-Phase im Merge WAEHLT — ACM, das BREITE vordere
   * Auto-Lock-Volumen (FBMig29Radar::kAcm*). Die dokumentierten CC/VS/BORE sind Azimut-BLEISTIFTE (das
   * "nur Vertikalachse" von DCS-FM p.12) und faengen einen manoevrierenden Merge-Gegner nicht — gemessen,
   * duel-merge lock_s 0. ACM nimmt die ±37° derselben T4-Quelle als AZIMUT (radar-sensors.md §7.1, der
   * Konflikt ist dort vermerkt) und ist Doppler-ausgenommen wie CC: genau die low-closure-Bedingung eines
   * co-speed Merges. Der Auto-Lock macht die Designation, die im Nahkampf niemand von Hand faehrt.
   * doc/modules/mig29/module.md gap 4h (a). */
  int BfmRadarModeOrdinal() const override { return (int)FBMig29RadarMode::Acm; }
  /* [DOC DCS-EA p.89] The set's own range-scale ladder is 54 / 27 / 13.5 / 5.4 nm; 13.5 nm is the step
   * at which the scale switches for the last time before the close-combat scale, and the radar's own
   * documented search reach is 27 nm, so the lock is taken one ladder step inside it. That is the same
   * reasoning the F-16 uses (lock inside the gate, outside every head-on Rtr) with this jet's numbers —
   * and here it costs more, because on this aircraft radiating at all is already the giveaway. */
  double InterceptLockRangeNm() const override { return 13.5; }
  /* [DOC T4 §7.1] The antenna's own azimuth gimbal is ±65°, so a crank has to stay inside it with the
   * reserve a manoeuvring target needs — the same construction as the F-16's 60-15. */
  double InterceptCrankAtaDeg() const override { return 50.0; }
  /* TRUE, and it is this aircraft's defining tactical property rather than a preference. The R-27R is
   * SEMI-ACTIVE: it homes on the reflection of THIS radar's illumination, so a beam manoeuvre does not
   * merely hide the jet — it takes the missile's only source of energy away
   * (doc/modules/mig29/weapons.md §3.2, consequence 4). The support obligation therefore runs to
   * IMPACT and not to a seeker activation that never comes, and while it runs the pilot does not turn
   * away. That is exactly the "jousting" the source names as the emergent behaviour of SARH-armed
   * aircraft: both illuminate, and neither may break first. */
  bool SupportInhibitsDefend() const override { return true; }
  /* [ABL] The BVR cruise speed, and the unit is the point: the AP speed loop controls TRUE airspeed
   * (systems/FBAutopilot's TargetSpeedMs against fb_fdm_state::speed). The F-16's rule, stated in
   * doc/modules/f16/module.md, is "the TAS whose CAS at the 8,000 m band IS the measured corner
   * speed" — 550 kt TAS = ~375 KCAS against a measured corner of 380 — because that number is two
   * things at once: the launch speed the round inherits into the Raero integration, and a jet already
   * at its best turn rate when the engagement stops being BVR. Applied to THIS airframe's measured
   * corner of 420 KCAS (anchor M2-VC), the same rule gives 600: [MESS, duel-headon at 8,000 m] the
   * loop settles at 422.3 KCAS / M 1.00 on a mean throttle of 0.45.
   *
   * IT USED TO BE 330, and that was a unit error rather than a taste: the derivation compared this
   * jet's corner in CAS against the F-16's ROUTE speed (300) instead of its intercept speed (550), and
   * then fed the CAS answer to a TAS command. [MESS] the MiG therefore cruised to every BVR merge at
   * 217 KCAS / M 0.54 — 40 % below its own BfmMinSpeedKt (380 KCAS, the speed below which the same
   * hooks say it departs in a hard turn), with the smaller launch zone that a slower launcher gets. */
  double InterceptSpeedKt() const override { return 600.0; }
  /* [DOC] "nose raise 125...135 kts" — the midpoint, and [MESS] anchor B1 rotated at 130.1 kt when the
   * stage-1 harness commanded exactly this. NO WEIGHT TABLE: the F-16 has one because doc/f16 prints
   * one; nothing in doc/mig29/ makes Vr a function of weight, so none is invented. [GAP] */
  double RotationSpeedKt(double grossWeightLbs) const override { (void)grossWeightLbs; return 130.0; }
  /* ZERO, and that is a statement rather than an omission: the F-16's 15 kt lead exists because its
   * source quotes Vr as the speed at which the jet is already FLYING, whereas "nose raise 125...135" is
   * the speed at which the stick comes back. Leading it would rotate this aircraft 15 kt early. */
  double RotationLeadKt() const override { return 0.0; }
  double RotationPitchDeg() const override { return 9.0; }    /* [DOC] 8...10 deg on the HUD */
  /* [DOC] doc/mig29/procedures.md §1 note 5: the FC3 voice warning implies 250 kts gear-down. */
  double GearUpLimitKt() const override { return 250.0; }
  /* [DOC] "at 270 kts set RPM 83-85 %, hold until clear of the control zone". The phase ends when the
   * gear is up, so this is the climb-out schedule and not a cruise number. */
  double ClimbSpeedKt() const override { return 270.0; }
  double TakeoffThrottleNorm() const override { return 1.0; } /* [DOC] release into full augmentation */

  /* [ABL] The Approach phase holds ONE speed from glideslope capture to the flare, and the source gives
   * two: ">= 175 kts final" (a PATTERN speed, flown from base) and "touchdown ~140 kts at 11 deg AoA".
   * A single held speed cannot be 175 and still touch down at 140 through a 20-30 ft flare, so it is
   * placed between them: 150 kt is 9.6 % above the [MESS] anchor M1 (11 deg AoA sits at 136.8 KCAS,
   * landing config, 12 t), which puts the glidepath at ~9 deg AoA — inside the documented 15 deg
   * base-leg cap with room, and close enough that the flare has ~10 kt to bleed rather than ~35. */
  double ApproachSpeedKt() const override { return 150.0; }
  double GlidepathAngleDeg() const override { return 3.0; }   /* [SET] no source states one */
  /* ZERO because the DECK forbids it: fcs/speedbrake-permitted is 0 whenever gear-pos-norm > 0.05
   * ([DCS-EA p.57]: extension is inhibited with the gear down). Commanding 0.5 here would be a control
   * input with no effect, which reads in telemetry as a working speedbrake. */
  double ApproachSpeedbrakeNorm() const override { return 0.0; }
  /* [DOC] "flare initiation 20...30 ft AGL" — the TOP of the band, because the flare is also what bleeds
   * the ~10 kt between approach and touchdown speed and the extra 10 ft is the only time available. */
  double FlareStartAglFt() const override { return 30.0; }
  /* [DOC] touchdown at 11 deg AoA. Held as a PITCH attitude, which in the flattening flare is the same
   * angle; [DOC] "do not exceed 13 deg AoA" and the deck's own nozzle-strike geometry (13.5 deg of
   * pitch on the mains, mig29.xml <ground_reactions>) leave 2.5 deg of margin above it. */
  double FlareTargetPitchDeg() const override { return 11.0; }
  /* [ABL] 3.5 deg below the deck's 13.5 deg nozzle strike and 5 deg below core/FBFlightMonitor's 15 deg
   * contact-pitch KO. [DCS-EA p.78] names the nozzle strike as a real consequence of full aft stick, so
   * the aerobrake attitude is bounded by this aircraft's geometry and not by the F-16's 12 deg. */
  double AerobrakePitchDeg() const override { return 10.0; }
  double AerobrakeSpeedKt() const override { return 115.0; }  /* [DOC] brake application at 115 kts */
  double RolloutBrakeNorm() const override { return 0.8; }    /* [SET], the F-16's, and the drag chute
                                                               * deploys itself in the deck below 175 kt */

  /* BFM — [MESS] `make -C sim test-mig29`, anchors M2-VC/M2-G: the corner sweep (5,000 m, 13 t, 85 deg
   * of bank, the documented 26 deg / 9 g limits) peaks at 420 KCAS with 24.2 deg/s and a mean 7.83 g.
   * NOT REACHABLE THIS ROUND — stage 2a composes no radar, so pilot/FBBfmTrack never gets a contact and
   * the Bfm phase cannot be entered. They are here because they are measured, and because leaving the
   * generic 300 kt / 4 g placeholders would put an F-16-sized energy schedule on this airframe the
   * moment stage 2c switches the sensor on. */
  double BfmCornerSpeedKt() const override { return 420.0; }
  double BfmCornerG() const override { return 7.8; }
  double BfmMaxG() const override { return 9.0; }             /* [DOC] +9 g [DCS-FM p.14] */
  /* [MESS] the same sweep departed (alpha peak > 35 deg) at every entry from 300 to 360 kt and held at
   * 380 and above: below that the SOS stand-in cannot hold 85 deg of bank on this deck. */
  double BfmMinSpeedKt() const override { return 380.0; }
  /* [MESS] THIS AIRFRAME'S OWN ROLL PLANT, identified exactly as the F-16's was: an ARX(1) fit
   * p[n+1] = a*p[n] + K*(1-a)*u[n] on 10 Hz samples of commanded roll against the differentiated bank
   * angle, taken from this jet's own BFM runs. a = 0.819 (roll time constant 0.50 s against the F-16's
   * 0.32), K = 201 deg/s per full deflection against 78.7 — this aircraft rolls 2.6x harder for the
   * same stick, which is why the generic (F-16) numbers turned the rate cap into a limit cycle here.
   * Cross-check against an INDEPENDENT measurement of the same airframe: `make -C sim test-mig29`'s
   * open-loop roll anchor reads 157.8 deg/s at 300 kt and 240.9 deg/s at 450 kt with full stick, so a
   * closed-loop 201 deg/s at BFM speeds sits between them where it should. */
  double BfmRollPlantA() const override { return 0.819; }
  double BfmRollPlantKDegS() const override { return 201.0; }
  /* [SET] Der Rolldeckel im SUCHLAUF, und er ist die Konsequenz aus BfmRollPlantKDegS: diese Zelle
   * rollt 2,6x haerter je Vollausschlag, also treibt ein Such-aim, das mit voller F-16-Rollautoritaet
   * geflogen wird, ihre Lage in einen Roll-Grenzzyklus (gemessen, mig29-bfm/duel-merge: Dauerrolle bei
   * gesunder Energie, nie ein Kontakt, Monitor-Departure). 0,20 * K(201) = ~40 deg/s stationaere
   * Rollrate — gerade genug, dem gebrieften Vektor / Datum zu folgen und den Scan-Weave zu fliegen,
   * sanft genug, dass die Nase steht und die N019 (RAD) den Kontakt aufbaut. Nur der SUCHLAUF ist
   * gedeckelt; der Kampfzug nach dem Lock rollt voll. doc/pilot.md 5.4. */
  double BfmSearchRollCap() const override { return 0.20; }
  /* [SET] Der kommandierte Rollraten-Deckel, tiefer als die F-16-90 aus demselben Grund wie
   * BfmSearchRollCap: K=201 laesst die Rate zwischen zwei 10-Hz-Entscheidungen auf ~118 deg/s
   * ueberschiessen (gemessen, mig29-bfm), die Verfolgung PIOt in eine Dauer-Rollrate ueber der
   * 60-deg/s-Departure-Schwelle des Monitors. 45 deg/s laesst nach dem Ueberschuss noch Rand zur
   * Schwelle und reicht fuer eine Kampfrolle auf dieser Zelle. doc/pilot.md 5.7. */
  double BfmRollRateMaxDegS() const override { return 60.0; }

private:
  /* The briefed transmissions, consumed from the front in brief order — the same array-with-a-cursor
   * shape as FBPilot's release and chaff briefs, for the same reason: nothing is rewritten in the tick
   * and the order is the file's. */
  struct GciCall {
    double AtS = 0.0, BrgDeg = 0.0, RangeM = 0.0, AltM = 0.0;
  };
  GciCall Gci_[kMaxGciCalls]{};
  int GciCount_ = 0, GciNext_ = 0;
  int GciStep_ = 0;               /* which of the three entries of the current call is still to type */
  double GciClockS_ = 0.0;        /* this pilot's own mission clock; FBPilot's is private, by design */
  double GciLastEntryS_ = -1e9;
};

} // namespace FlightBox::Modules
#endif
