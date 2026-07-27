/* FlightBox — the avionics COMMAND value types: what a pilot (human or AI) can ask the jet to do, and
 * what the jet answers. The shape is taken from the DED's documented propose -> commit/reject protocol
 * (doc/f16/controls-commands.md §"The DED's propose -> commit/reject protocol"): a command is
 * {target, proposed value}, the receipt is {committed, reason}. Nothing here executes anything — the
 * OWNING system does that in its own tick and answers; this file is the vocabulary.
 *
 * WHY A COMMAND PATH AT ALL, when the AI could just call the setter. Because a setter call is not a
 * pilot action: it cannot be refused, it cannot take time, it cannot happen while both hands are busy
 * flying, and it leaves no trace. Every one of those four is a real property of the cockpit, and all
 * four are what this model reintroduces — an AI that steers the jet through the same vocabulary a human
 * has is an AI whose advantage is decision quality, not access. */
#ifndef FBAVIONICSCOMMAND_H
#define FBAVIONICSCOMMAND_H

#include <cstdint>

namespace FlightBox {

/* WHAT can be commanded. Ordinals are telemetry-visible — append, never reorder. */
enum class FBCommandTarget : uint8_t {
  None = 0,
  RadarMode, RadarRangeNm, RadarSlewAz, RadarSlewEl, IffTransponder, IffInterrogator,
  DatalinkPower, DatalinkTransmit, DatalinkFilter, DatalinkRangeNm,
  MasterMode, MasterArm, AlowFt, BingoLbs, SteerpointNum,
  WeaponSelect, Designate,
  /* The stores pair. StationSelect steps the SMS to a pylon; WeaponRelease is the pickle — the ONE way
   * a store leaves the aircraft, so that a release can be refused, takes a hand's worth of time and
   * leaves a receipt like every other cockpit action (this file's banner). Appended, never inserted:
   * the ordinals above are telemetry-visible in every run ever measured. */
  StationSelect, WeaponRelease,
  /* The defensive trio (doc/f16/defence-rwr-cm.md §2.2/§2.4). CmDispense is the CMS switch's forward
   * throw — the one action a pilot performs IN a manoeuvre rather than between two, which is exactly
   * why it is a HOTAS-class command and not a panel edit (value 0 = the program the PRGM knob selects,
   * 1..6 = a program directly). CmConsent is CMS Aft/Right, the SEMI/AUTO consent. CmdsMode is the mode
   * knob on the left console: a hand off the throttle and a head down, i.e. the DED class. Appended,
   * never inserted. */
  CmDispense, CmConsent, CmdsMode,
  /* THE TRIGGER. A HOTAS action like the pickle, and the ONE way rounds leave the gun — so a burst can
   * be refused (safe, on the ground, empty drum), costs a finger's worth of time and leaves a receipt,
   * exactly like every other cockpit action. Its VALUE is the length of the squeeze in seconds
   * (systems/FBGunSystem's banner): a command models one action, and one action on a trigger is one
   * burst of a stated length — not a stream of press/release commands the bus's own 0.5 s floor between
   * two actions on the same switch could never carry. Appended, never inserted. */
  GunTrigger,
};

const char *FBCommandTargetStr(FBCommandTarget t);

/* THE TWO LATENCY CLASSES (doc/f16/controls-commands.md §5, the one quantitative timing material in
 * the source guides). A HOTAS action is one press or one switch throw: sub-second, usable in the middle
 * of a manoeuvre, and its own avionics uses 0.5 s and 1.0 s hold times to tell two commands on the same
 * switch apart. A DED field edit is select -> type -> ENTR: several seconds, head DOWN, hands off — the
 * class of thing a pilot does BETWEEN manoeuvring segments, never during one. Modelling them as one
 * class would let an AI type a steerpoint while pulling 7 g, which is the specific thing this split
 * exists to forbid. */
enum class FBCommandClass : uint8_t { Hotas, Ded };

FBCommandClass FBCommandClassOf(FBCommandTarget t);

/* WHICH system owns the target — the module dispatches a due command to the slot that owns it, in that
 * slot's own tick, so a command is consumed at the cadence of the box that answers it. */
enum class FBCommandGroup : uint8_t { Sensors, Comms, Avionics, Stores, Defensive };

FBCommandGroup FBCommandGroupOf(FBCommandTarget t);

/* HOW IT ENDED. Four outcomes, because the source material documents four distinguishable endings and
 * collapsing them into a bool would throw away the interesting ones:
 *   Accepted   — committed, effect live.
 *   Clamped    — committed and the field shows what was typed, but a system ceiling governs the EFFECT
 *                (§6.8, the BNGO ceiling). Not a rejection: ENTR succeeded.
 *   Inhibited  — committed, effect gated by something else (§6.4, ALOW without a powered radalt).
 *   Rejected   — not committed. Reason says why.
 * Pending is the in-flight state between Post() and the owning system's answer. */
enum class FBCommandOutcome : uint8_t { Pending, Accepted, Clamped, Inhibited, Rejected };

const char *FBCommandOutcomeStr(FBCommandOutcome o);

/* WHY. The first eight entries are the eight documented rejection/precondition patterns of
 * doc/f16/controls-commands.md §6, one to one and in its order. The last three are FlightBox's OWN, and
 * are marked as such because the guides document none of them:
 *   OutOfRange  — the numeric bounds policy the source material explicitly lacks (see §6's closing
 *                 note: "a FlightBox command-block model will need to invent its own range-validation
 *                 policy"). Ours REJECTS and says so rather than clamping silently; the one documented
 *                 clamp (BNGO) is modelled as Clamped above, so silence is never an outcome.
 *   ChannelBusy — the latency model's own answer: the pilot's hands/head are already occupied with an
 *                 uncompleted command of the same class. Derived from §5's press-duration ceiling, not
 *                 stated by any guide. */
enum class FBCommandReason : uint8_t {
  None = 0,
  PilotReject,            /* §6.1 RCL/RTN — the pilot changed their mind */
  HardwarePrecedence,     /* §6.2 a physical switch position locks the software path out */
  SequencePrecondition,   /* §6.3 a state-machine order (roll AP mode needs a pitch mode first) */
  EffectPrecondition,     /* §6.4 accepted, but the EFFECT needs something else (ALOW/radalt) */
  OutOfContext,           /* §6.5 the silent no-op: valid command, wrong SOI/mode */
  NotImplemented,         /* §6.6 the box exists in the jet but not (yet) in this simulator */
  SoftFailure,            /* §6.7 succeeded but corrupted state (DTE MPD upload w/ CMDS not STBY) */
  ValueClamped,           /* §6.8 accepted, system ceiling governs */
  OutOfRange,             /* FlightBox policy — see the enum banner */
  ChannelBusy,            /* FlightBox policy — see the enum banner */
  /* FlightBox's own third: the box is willing and the command is valid, but the magazine behind it is
   * empty. Distinct from OutOfContext (which says the command made no sense here) because an empty
   * dispenser is a fact about the AIRCRAFT that the pilot needs to hear differently — and it is the one
   * refusal a defensive system produces in the middle of being shot at. */
  Depleted,
  /* ...and its fourth: the box the command is addressed to is GONE — shot away, not switched off
   * (core/FBSystemHealth). Its own reason because it is neither a context error nor a precondition the
   * pilot can satisfy: nothing about the aircraft's configuration will make this command work again, and
   * a cockpit that answered "wrong mode" to a destroyed radar would send its pilot looking for a switch
   * that no longer does anything. */
  SystemFailed,
};

const char *FBCommandReasonStr(FBCommandReason r);

/* One issued command. `Value` carries every payload as a double — enum selections travel as their own
 * ordinal, booleans as 0/1 — because a command queue with a variant payload would need a tag and a
 * switch at every hop for no gain: the TARGET already says how to read the number. */
struct FBAvionicsCommand {
  uint32_t Seq = 0;
  FBCommandTarget Target = FBCommandTarget::None;
  double Value = 0.0;
  double IssuedS = 0.0;
  double DueS = 0.0;   /* IssuedS + the class latency: not consumable before this (see FBCommandClass) */
};

/* The receipt. Kept separate from the command so a consumer can log/telemeter the pair without the
 * queue entry having to survive. */
struct FBCommandAck {
  uint32_t Seq = 0;
  FBCommandTarget Target = FBCommandTarget::None;
  double Value = 0.0;
  FBCommandOutcome Outcome = FBCommandOutcome::Pending;
  FBCommandReason Reason = FBCommandReason::None;
  double CompletedS = 0.0;

  bool Committed() const { return Outcome != FBCommandOutcome::Rejected && Outcome != FBCommandOutcome::Pending; }
};

} // namespace FlightBox
#endif
