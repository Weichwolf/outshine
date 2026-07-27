/* FlightBox — FBEngagement: ONE beyond-visual-range engagement as the pilot flying it can see it — the
 * state it is in, the decisions it has already taken, and the numbers a later selection process will
 * grade it on. Owned by systems/FBPilot, inert until its Intercept phase feeds it.
 *
 * IT IS THE SCOREBOARD, NOT THE BRAIN, and the split matters. systems/FBBfmTrack holds the same two
 * roles for the fight (picture + metrics) because a tracker has to be one object; here the PICTURE is
 * already on the bus (the radar block, the RWR block, the fire-control block) and what is missing is a
 * place to remember what happened: when the target was first seen, when it was first locked, what the
 * launch zone said at the instant the trigger was pulled, whether the lock survived long enough for the
 * round to go active, and how long the pilot took to answer a warning. FBPilot::Run decides; this class
 * records, and publishes the record as telemetry.
 *
 * WHY THESE CHANNELS AND NOT OTHERS. They are the intended FITNESS of an evolved intercept pilot, so
 * each one has to be (a) computable from this aircraft's own instruments — nothing here reads the other
 * jet's truth, exactly like FBBfmTrack — and (b) a quantity a real debrief would argue about:
 *   TIME TO ACQUISITION (eng_detect_s, eng_lock_s) — how quickly the sensor was made to find him. A
 *     pilot who points the antenna at the wrong altitude finds him late or not at all.
 *   THE SHOT (eng_shot_s/_nm/_aspect + the zone it was taken in) — a shot is only as good as the
 *     geometry it was taken in, so the launch zone AT LAUNCH is recorded beside the range. Shooting at
 *     Raero is a shot the target can simply outrun; shooting inside Rtr is one it cannot.
 *   SUPPORT (eng_support_s, eng_support_f, eng_pitbull) — the discriminator between a launch and a
 *     kill. An AIM-120 flies its midcourse on the launcher's uplink; a pilot who turns away too early
 *     has fired a round that will arrive somewhere near where the target used to be going.
 *   REACTION (eng_threat_s, eng_react_s) — the seconds between the warning and the first defensive
 *     command. It is the one channel that is purely about the PILOT rather than the geometry.
 *   ENERGY (eng_es, eng_es_min) — energy height, the same quantity FBBfmTrack publishes, because the
 *     state you leave an engagement in decides whether there is a second one.
 * All of them survive the engagement, so the LAST ROW of a run is the whole debrief. */
#ifndef FBENGAGEMENT_H
#define FBENGAGEMENT_H

#include "FBTelemetry.h"

namespace FlightBox {

/* The intercept's own state machine (systems/FBPilot's Intercept phase drives it). Telemetry-visible
 * strings — append, never reorder.
 *   Idle      not intercepting.
 *   Search    nothing on the scope worth pointing at: fly the briefed vector, search, DO NOT LOCK.
 *   Closing   a contact is being worked in search mode — still no lock, because a lock is a warning.
 *   Attack    locked: the launch zone is being read and the trigger is waiting for the geometry.
 *   Support   a round is in the air: hold the lock, crank away as far as the antenna allows.
 *   Defend    somebody has a solution on this aircraft: beam him and throw chaff.
 *   Abort     leave. Out of weapons, out of fuel, or too close to be doing this at all. */
enum class FBEngageState { Idle, Search, Closing, Attack, Support, Defend, Abort };
const char *FBEngageStateStr(FBEngageState s);

class FBEngagement : public FBTelemetrySource {
public:
  /* ---- the per-tick picture the pilot hands down (what it can see right now) ---- */
  void Report(FBEngageState state, bool haveTarget, bool locked, double rangeM, double ataDeg,
              double aspectDeg, double closureMs, double esFt, double dt);

  /* ---- the events, each recorded exactly once (first occurrence wins) ---- */
  void NoteContact(double nowS);          /* first firm radar contact on the engaged target */
  void NoteLock(double nowS);             /* first single-target track */
  /* The trigger, with the whole launch zone as the fire control reported it at that instant — the
   * prediction the flown result is later measured against (modules/f16/FBF16FireControl). */
  void NoteShot(double nowS, double rangeM, double ataDeg, double aspectDeg, double raeroM, double rtrM,
                double rminM, double ttaS, double ttiS);
  void NoteThreat(double nowS);           /* first track-class warning on the receiver */
  /* The first command posted in answer to a warning, measured from the CUE — the moment the warning
   * became one the pilot judged had to be answered — and not from the first symbol ever seen. A pilot
   * who presses on through a lock spike and defends only against the launch has reacted to the launch;
   * timing that from the earlier spike would score the decision as a two-minute reaction. */
  void NoteDefensiveAction(double nowS, double cueS);
  /* Cartridges the dispenser actually ejected since the last tick — the CMDS block's own count, not
   * the number of switch throws: a program is a burst, and what a debrief cares about is how much of
   * the magazine the defence cost. */
  void NoteChaff(int n);

  /* One tick inside the SUPPORT WINDOW — the [launch, launch + predicted time-to-active] interval the
   * round needs the launcher's uplink for. `locked` says whether the uplink is actually being fed.
   * Bounded on purpose: support measured past the window would count guidance nobody asked for and
   * would put eng_support_f above 1, which is not a fraction of anything. */
  void NoteSupport(bool locked, double nowS, double dt);

  bool HaveShot() const { return ShotS_ >= 0.0; }
  double ShotS() const { return ShotS_; }
  double ShotTtaS() const { return ShotTtaS_; }
  double ShotTtiS() const { return ShotTtiS_; }
  int  Shots() const { return Shots_; }
  bool Pitbull() const { return Pitbull_; }
  double SupportS() const { return SupportS_; }

  void Reset();

  const char *TelemetryName() const override { return "eng"; }
  void DeclareTelemetry(FBTelemetrySchema &schema) const override;
  void SampleTelemetry(FBTelemetryRow &row) const override;

private:
  FBEngageState State_ = FBEngageState::Idle;
  bool   Have_ = false, Locked_ = false;
  double RangeM_ = 0.0, AtaDeg_ = 0.0, AspectDeg_ = 0.0, ClosureMs_ = 0.0;
  double EsFt_ = 0.0, EsMinFt_ = 0.0;
  bool   HaveEs_ = false;

  double EngagedS_ = 0.0, DefendS_ = 0.0;
  double DetectS_ = -1.0, LockS_ = -1.0;
  double ShotS_ = -1.0, ShotRangeM_ = -1.0, ShotAtaDeg_ = 0.0, ShotAspectDeg_ = -1.0;
  double ShotRaeroM_ = -1.0, ShotRtrM_ = -1.0, ShotRminM_ = -1.0, ShotTtaS_ = -1.0, ShotTtiS_ = -1.0;
  double SupportS_ = 0.0;
  bool   Pitbull_ = false, SupportDone_ = false;
  double ThreatS_ = -1.0, ReactS_ = -1.0;
  int    Shots_ = 0, Chaff_ = 0;
};

} // namespace FlightBox
#endif
