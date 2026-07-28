/* The avionics COMMAND vocabulary: {target, proposed value} in, {committed, reason} out — the DED's
 * documented propose->commit/reject protocol. Nothing here executes: the OWNING system answers in its
 * own tick. Why a command path instead of a setter (refusable, costly, traceable, blockable):
 * doc/core.md, Abschnitt 2. */
#ifndef FBAVIONICSCOMMAND_H
#define FBAVIONICSCOMMAND_H

#include <cstdint>

namespace FlightBox {

/* Ordinals are telemetry-visible — append, never reorder. Class/group per target: FBCommandBus.cpp. */
enum class FBCommandTarget : uint8_t {
  None = 0,
  RadarMode, RadarRangeNm, RadarSlewAz, RadarSlewEl, IffTransponder, IffInterrogator,
  DatalinkPower, DatalinkTransmit, DatalinkFilter, DatalinkRangeNm,
  MasterMode, MasterArm, AlowFt, BingoLbs, SteerpointNum,
  WeaponSelect, Designate,
  StationSelect, WeaponRelease,          /* WeaponRelease is the pickle: the ONE way a store leaves */
  CmDispense, CmConsent, CmdsMode,       /* CmDispense value: 0 = the PRGM knob's program, 1..6 direct */
  GunTrigger,                            /* the ONE way rounds leave; VALUE = squeeze length in seconds */
  /* The PASSIVE optical head (sensors/FBIrstSystem). Appended, never inserted: the ordinal is a
   * telemetry column value. IrstLaser is the only ACTIVE thing that station does — and the only
   * command in this table whose effect is a measurement rather than a state. */
  IrstMode, IrstDesignate, IrstLaser,
  /* The EMISSION control of a radar that has one as a separate switch from power (ILLUM/DUMMY/OFF on
   * the MiG-29's PUR-31). Value = the module's own ordinal, like RadarMode. */
  RadarEmission,
};

const char *FBCommandTargetStr(FBCommandTarget t);

/* THE TWO LATENCY CLASSES: a HOTAS throw is sub-second and usable mid-manoeuvre, a DED edit is
 * select->type->ENTR with the head down. One class would let an AI type a steerpoint at 7 g — the
 * specific thing this split forbids (doc/modules/f16/controls-commands.md §5). */
enum class FBCommandClass : uint8_t { Hotas, Ded };

FBCommandClass FBCommandClassOf(FBCommandTarget t);

/* WHICH system owns the target — a command is consumed at the cadence of the box that answers it. */
enum class FBCommandGroup : uint8_t { Sensors, Comms, Avionics, Stores, Defensive };

FBCommandGroup FBCommandGroupOf(FBCommandTarget t);

/* Four outcomes because the source documents four distinguishable endings: Accepted (effect live),
 * Clamped (committed, a system ceiling governs the EFFECT — ENTR succeeded), Inhibited (committed,
 * effect gated elsewhere), Rejected. Pending is the state between Post() and the owner's answer. */
enum class FBCommandOutcome : uint8_t { Pending, Accepted, Clamped, Inhibited, Rejected };

const char *FBCommandOutcomeStr(FBCommandOutcome o);

/* The first eight are doc/modules/f16/controls-commands.md §6's documented patterns, one to one and in its
 * order; the last four are FlightBox's OWN and marked as such (doc/core.md, Abschnitt 2.4). */
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
  OutOfRange,             /* FlightBox: reject rather than clamp silently */
  ChannelBusy,            /* FlightBox: hands/head still busy with a command of the same class */
  Depleted,               /* FlightBox: willing box, empty magazine — a fact about the AIRCRAFT */
  SystemFailed,           /* FlightBox: the box is GONE — shot away, not switched off. No configuration
                           * change brings this command back, so it is not a precondition */
};

const char *FBCommandReasonStr(FBCommandReason r);

/* `Value` carries every payload as a double (enums as their ordinal, booleans as 0/1): the TARGET
 * already says how to read the number, so a variant payload would only add a tag and a switch. */
struct FBAvionicsCommand {
  uint32_t Seq = 0;
  FBCommandTarget Target = FBCommandTarget::None;
  double Value = 0.0;
  double IssuedS = 0.0;
  double DueS = 0.0;   /* IssuedS + the class latency: not consumable before this (see FBCommandClass) */
};

/* Separate from the command so a consumer can log the pair without the queue entry surviving. */
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
