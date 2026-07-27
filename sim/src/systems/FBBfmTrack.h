/* FlightBox — FBBfmTrack: the pilot's own picture of the jet it is fighting, built from RADAR CONTACTS
 * AND NOTHING ELSE, plus the BFM metrics derived from it. Owned by systems/FBPilot; inert until the
 * pilot's BFM phase feeds it.
 *
 * WHY THE PILOT NEEDS A TRACKER AT ALL. A radar contact (core/FBRadarContact.h) is an echo: range,
 * direction, closure. It does not say where the target is GOING, and every pursuit decision in BFM is
 * about exactly that — lead means "aim where he will be", lag means "aim where he has been", aspect
 * means "how far off his tail am I". So the velocity has to be ESTIMATED from successive looks, which is
 * what a fighter pilot does in his head and what the FCR's own trackfile does on the MFD. This class is
 * that estimate: an alpha-filtered position/velocity track that is updated only when a genuinely NEW
 * look arrives (FBRadarContact::LookAgeS marks the look's age, so a re-read of the same frozen look must
 * not be differenced into a zero velocity) and EXTRAPOLATED forward the rest of the time.
 *
 * That extrapolation is not a convenience — it is the whole sensor-limitation story. A dropped lock does
 * not delete what the pilot knows; it starts a clock. Block() keeps answering from the predicted
 * position while AgeS counts up, and stops being Valid only past kMaxExtrapolateS, when the prediction
 * has outlived its usefulness and the pilot has to go looking (FBPilot's search pattern).
 *
 * WHAT IT MAY READ: FBState's radar block (contacts + lock index) and the pilot's own fb_fdm_state. There is no
 * FBWorld, no unit registry and no datalink track in this file's includes, and that is the anti-cheat
 * property the BFM proof rests on (CLAUDE.md "Kein Cheaten"): the position it reports is a computed
 * estimate that can be — and in the mission analysis IS — compared against the truth it never saw.
 *
 * IT IS ALSO THE SCOREBOARD (FBTelemetrySource "bfm"). The metrics an evolutionary pilot will later be
 * selected on are exactly the quantities this class already holds, so they are published from here
 * rather than recomputed by an analysis that would then be judging its own copy of the geometry: aspect
 * angle, angle off the nose, range, closure, own specific energy, and the two integrals that make a run
 * comparable to another — seconds with the lock held and seconds in the control position, both against
 * seconds engaged. Everything here is computable FROM THE UNIT'S OWN PERSPECTIVE; anything that needs
 * the other jet's truth belongs to the analysis, not to this file. */
#ifndef FBBFMTRACK_H
#define FBBFMTRACK_H

#include "FBFdm.h"
#include "FBState.h"
#include "FBTelemetry.h"

namespace FlightBox {

/* What the pilot decided to do with the picture this tick. Telemetry ordinal — append, never reorder. */
enum class FBBfmPursuit { None, Search, Lead, Pure, Lag };
const char *FBBfmPursuitStr(FBBfmPursuit p);

/* The fused picture is published as core/FBAvionicsBlocks.h's FBBfmBlock — a bus block like any other
 * sensor product, so the pilot's steering never needs a "do I have a live contact" branch of its own:
 * the head says it. The three states map onto the fusion's own three, which is why they exist:
 *   Invalid — never seen. Nothing to steer at.
 *   Valid   — measured, or extrapolated within the credible window: young enough to LEAD on.
 *   Held    — past kMaxExtrapolateS. The estimate reverts to the last MEASURED position and the block
 *             freezes there (where he WAS beats where a stale velocity says he would be) — the same
 *             freeze-at-last-value the CRUS page does, arrived at from a completely different problem.
 * The stamp is the LOOK the estimate stands on, so age is age since the sensor last saw him. */

/* THE DATUM: where a target that is no longer being seen COULD be now, and how big "could" has become.
 *
 * The block above answers "where is he" and stops answering it honestly past kMaxExtrapolateS, when it
 * freezes on the last measured position. That freeze is right for STEERING A PURSUIT — a pilot must not
 * pull lead on a guess — and useless for the other thing a pilot does with a lost target, which is go
 * back and look for him. Looking needs two numbers the block does not carry: a POINT to fly to and the
 * WIDTH of the region around it, because that width is what says how far the nose has to be swept.
 *
 * BOTH COME OUT OF THE SAME ESTIMATE, with one assumption added: that the other jet can turn about as
 * hard as this one (FBPilot::CornerTurnRateDegS, derived from its own measured corner numbers). A target
 * last seen at speed V, turning at rate w, is after t seconds displaced from the straight-line
 * prediction by (V/w)*sqrt((wt - sin wt)^2 + (1 - cos wt)^2), which for the times that matter is
 * 0.5*V*w*t^2 — and it can never be further than V*t from where it was actually seen, whatever it did.
 * So the uncertainty radius is min(0.5*V*w*t^2, V*t), and the two halves cross at t = 2/w: BEFORE that
 * the prediction is worth more than the last look, AFTER it the turn could have gone anywhere and the
 * honest centre stops moving. That crossing is derived, not chosen, and for this simulator's F-16 it
 * lands at ~7 s — which is where kMaxExtrapolateS above was independently put at 8 s.
 *
 * HalfWidthDeg is that radius seen from here: the half-angle a search has to cover, which is what makes
 * a scan pattern a consequence of what the pilot knows instead of a fixed weave. */
struct FBTrackDatum {
  bool   Valid = false;         /* something was measured at some point; nothing to go back to if not */
  double AgeS = 0.0;            /* since the LOOK the estimate stands on */
  double EastM = 0.0, NorthM = 0.0, UpM = 0.0;   /* own-relative offset of the region's centre */
  double RangeM = 0.0;
  double BearingDeg = 0.0;      /* true bearing to it */
  double AltM = 0.0;            /* the centre's altitude, m ASL */
  double RadiusM = 0.0;         /* how big the region has grown */
  double HalfWidthDeg = 0.0;    /* ...as an angle from here */
};

class FBBfmTrack : public FBTelemetrySource {
public:
  /* Velocity is differenced between CONSECUTIVE looks and filtered, not measured over a longer baseline.
   * Against a target that is turning, lag beats noise: measured against the truth reconstructed from both
   * units' logs, consecutive looks with this filter put the estimated velocity DIRECTION 1.8 deg off
   * (median, p90 1.9) while a half-second baseline — five times less differencing error, half a second
   * more lag — was 5.4 deg off. One look per ~0.1 s STT frame makes 0.25 a ~0.4 s time constant: fast
   * enough to follow a reversal, slow enough that a single look cannot steer the jet. */
  static constexpr double kVelAlpha = 0.25;
  static constexpr double kMinLookDtS = 0.05;      /* below this two looks are the same look, numerically */
  /* How long a lost target is propagated before the estimate reverts to the last MEASURED position. A
   * constant-velocity model against a jet that is turning goes wrong fast — at a fighter's ~5 deg/s
   * sustained turn rate, eight seconds of straight-line prediction already puts the estimate most of a
   * turn diameter away from the truth, and past that the honest datum (where he WAS) beats the
   * extrapolation (where he would be if he had stopped manoeuvring, which he has not). */
  static constexpr double kMaxExtrapolateS = 8.0;
  static constexpr double kMinTrackSpeedMs = 20.0; /* below it a velocity vector has no meaningful heading */

  /* One decision tick. Reads the LOCKED contact only — an unlocked search return is a detection, not a
   * target the pilot has committed to, and mixing the two would make the pursuit jump between jets. */
  void Update(const FBState &state, const fb_fdm_state &own, double nowS);

  /* The pilot's own verdict on this tick, fed back for the scoreboard (the pilot owns the thresholds —
   * they are its airframe-specific numbers, not this container's). */
  void Report(FBBfmPursuit pursuit, bool inControl, double gCmd, double dt);

  /* Leaving BFM (or never entering it) must not leave a stale picture behind. */
  void Reset();

  /* The last known state as something to go LOOK for (see FBTrackDatum). `turnRateDegS` is how hard the
   * pilot assumes the target can turn — its own airframe's number, since this class knows no airframe. */
  FBTrackDatum Datum(const fb_fdm_state &own, double nowS, double turnRateDegS) const;

  const FBBfmBlock &Block() const { return Blk_; }
  bool Engaged() const { return EngagedS_ > 0.0; }
  double LockHeldFrac() const { return EngagedS_ > 0.0 ? LockS_ / EngagedS_ : 0.0; }
  double ControlS() const { return ControlS_; }

  const char *TelemetryName() const override { return "bfm"; }
  void DeclareTelemetry(FBTelemetrySchema &schema) const override;
  void SampleTelemetry(FBTelemetryRow &row) const override;

private:
  void Predict(const fb_fdm_state &own, double nowS);

  FBBfmBlock Blk_{};
  bool   Have_ = false;          /* at least one look has been folded in */
  double LastLookS_ = -1e9;      /* sim time of the look the estimate stands on */
  double PosLatDeg_ = 0.0, PosLonDeg_ = 0.0, PosAltM_ = 0.0;   /* estimated target position */
  double VelE_ = 0.0, VelN_ = 0.0, VelU_ = 0.0;

  /* Scoreboard (see the class banner). */
  FBBfmPursuit Pursuit_ = FBBfmPursuit::None;
  bool   InControl_ = false;
  double GCmd_ = 0.0;
  double AgeS_ = 0.0;            /* telemetry's copy of the head's age (SampleTelemetry has no clock) */
  double EsFt_ = 0.0;
  double EngagedS_ = 0.0, LockS_ = 0.0, ControlS_ = 0.0;
};

} // namespace FlightBox
#endif
