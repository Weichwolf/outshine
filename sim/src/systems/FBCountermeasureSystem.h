/* FlightBox — FBCountermeasureSystem: the active half of the Defensive slot, beside the passive
 * systems/FBRwrSystem. It owns what this aircraft can throw, the programs that decide HOW it is thrown,
 * the mode knob that decides WHO may throw it, and the clouds that are left hanging in the air
 * afterwards.
 *
 * A PROGRAM IS DATA, NOT BEHAVIOUR (core/FBCountermeasure.h): burst quantity, burst interval, salvo
 * quantity, salvo interval, per countermeasure type — the AN/ALE-47's own DED schema
 * (doc/f16/defence-rwr-cm.md §2.2), ranges included. This class is the machine that PLAYS one: it
 * ejects a cartridge, waits the burst interval, ejects the next, waits the salvo interval, and stops
 * when the salvos run out or the magazine does. Everything a mission wants to change about the pattern
 * is a number in a program, never a line of code here.
 *
 * THE MODE KNOB IS A STATE MACHINE, NOT A BOOLEAN (§2.2's table, which is the load-bearing part of the
 * source material and is reproduced here rather than summarised):
 *   OFF/STBY  nothing dispenses. STBY is the only mode in which programs may be reprogrammed.
 *   MAN       CMS Forward dispenses the program the PRGM knob selects. Nothing automatic.
 *   SEMI      the system PICKS the program for the threat, but every single dispense needs the pilot's
 *             consent (CMS Aft) and the prompt returns after each program completes.
 *   AUTO      the system picks AND repeats it while the threat lasts; consent is granted once, by
 *             turning the knob to AUTO, and CMS Right revokes it and interrupts what is running.
 *   BYP       exactly one chaff and one flare, nothing else.
 * SEMI and AUTO are meaningfully different machines — per-dispense consent versus consent-per-mode-entry
 * — and the source says so explicitly, so they are two states here and not one flag. Automatic
 * dispensing (and only automatic) is withheld while chaff is at its BINGO quantity, which is also the
 * source's rule.
 *
 * IT REACTS TO WHAT THE JET KNOWS, NOT TO WHAT IS TRUE. In SEMI/AUTO the trigger is the RWR block on the
 * shared bus (systems/FBRwrSystem writes, this reads) — i.e. this system fires at a WARNING, and if the
 * warning is missing (the emitter is in the receiver's blind zone, doc/f16/defence-rwr-cm.md §2.1) it
 * does nothing at all, which is precisely the compounding vulnerability the source describes. No path
 * from here reaches the world, the registry or another unit.
 *
 * WHAT A DISPENSE ACTUALLY DOES is the point of the whole file: a chaff cartridge leaves a CLOUD at the
 * aircraft's position (core/FBCountermeasure.h), published in the unit's signature, and an enemy radar
 * then has to decide between two returns. That decision belongs to the radar (systems/FBRadarSystem's
 * Doppler discrimination), not here — this class does not know whether it worked, exactly as the pilot
 * does not.
 *
 * FLARES ARE DISPENSED AND COUNTED AND HAVE NO EFFECT YET, and that is stated rather than hidden: an
 * infrared decoy needs an infrared seeker to fool, and this simulator has no IR sensor. The books are
 * kept honestly so that the day an AIM-9 exists, nothing here changes.
 *
 * INTERFACE + GENERIC DEFAULT in one class (the FBRadarSystem/FBRwrSystem pattern): Run() is the
 * override point and the threat->program mapping is the one protected hook; the magazine and the
 * program table are CONFIG a module installs in its constructor (the FBF16Fcr pattern, not an empty
 * derivation) — modules/f16/FBF16Cmds is the ALE-47's. */
#ifndef FBCOUNTERMEASURESYSTEM_H
#define FBCOUNTERMEASURESYSTEM_H

#include "FBAvionicsCommand.h"
#include "FBCountermeasure.h"
#include "FBState.h"
#include "FBTelemetry.h"
#include "FBFdm.h"

namespace FlightBox {

class FBCountermeasureSystem : public FBTelemetrySource {
public:
  /* PRGM 1-4 plus the slap-switch program 5 and the bypass program 6 (doc/f16/defence-rwr-cm.md §2.2). */
  static constexpr int kProgramCount = 6;
  static constexpr int kBypassProgram = 6;

  FBCountermeasureSystem();
  ~FBCountermeasureSystem() override = default;

  /* ---- setup, once, by the module or a mission line ---- */
  void SetMode(FBCmdsMode m);
  FBCmdsMode Mode() const { return Mode_; }
  /* The PRGM knob: which manual program CMS Forward plays. Programs 5 and 6 stay reachable regardless
   * (§2.2), which is why Dispense() takes a program number of its own. */
  bool SelectProgram(int n);
  int  SelectedProgram() const { return Selected_; }
  /* Reprogramming is allowed in STBY only (§2.2) — enforced here, so a mission that wants a different
   * pattern has to declare the jet in the state the real one has to be in. Returns false on an invalid
   * program number, an out-of-range parameter, or the wrong mode. */
  bool SetProgram(int n, const FBCmProgram &p);
  const FBCmProgram &Program(int n) const;

  void SetLoadout(int chaff, int flare);
  /* The CMDS BINGO page's per-type quantity, 0..99 (§2.2): at or below it the "LO" lamp lights and
   * AUTOMATIC dispensing is withheld. */
  bool SetBingo(int chaff, int flare);
  int  ChaffRemaining() const { return Chaff_; }
  int  FlareRemaining() const { return Flare_; }

  /* CMS Aft / CMS Right: consent in SEMI (per dispense) and AUTO (per mode entry). */
  void SetConsent(bool on);
  bool Consent() const { return Consent_; }

  /* ---- the trigger ----
   * Reached from the module's avionics command bus, never called directly (the same rule the pickle
   * follows, systems/FBStoresSystem::Release): a dispense can be refused, the refusal has a reason, and
   * both are on the record. `program` 0 = "the one the PRGM knob selects" (CMS Forward). */
  bool Dispense(int program, double nowS, FBCommandOutcome &outcome, FBCommandReason &reason);

  /* What this aircraft currently has hanging in the air — published in the unit's emission signature
   * (units/FBUnit.h) at the tick barrier, exactly like the datalink transmitter and the radar beam. */
  const FBChaffCloud *Clouds() const { return Clouds_; }
  int ActiveClouds() const { return ActiveClouds_; }

  /* The override point (class banner). `st` is this aircraft's own state — where a cartridge ends up is
   * where the aircraft was when it left — and `simTimeS` the module's absolute sim clock, which is what
   * the burst/salvo intervals are measured against (so a program plays at its declared rate however
   * often the module happens to cycle this slot). */
  virtual void Run(FBState &state, const fb_fdm_state &st, double simTimeS);

  const char *TelemetryName() const override { return "cm"; }
  void DeclareTelemetry(FBTelemetrySchema &schema) const override;
  void SampleTelemetry(FBTelemetryRow &row) const override;

protected:
  /* WHICH automatic program answers WHICH threat (SEMI/AUTO). The source documents that the system
   * "auto-selects the appropriate Automatic program per threat" and never which; the mapping is
   * therefore a module's doctrine, expressed as a hook rather than invented in the generic layer. */
  virtual int AutomaticProgram(FBRwrThreatMode worst) const;

  /* Programming without the STBY gate, for a module's own table (its constructor IS the ground crew).
   * The public SetProgram is the DED path and keeps the gate. Returns false on an invalid program. */
  bool InstallProgram(int n, const FBCmProgram &p);

  FBCmProgram Programs_[kProgramCount];

private:
  /* One type's playing head: which salvo, which cartridge inside it, and when the next one is due. */
  struct Player {
    bool   Running = false;
    int    SalvosLeft = 0;
    int    BurstLeft = 0;
    double NextS = 0.0;
  };

  void LoadGenericPrograms();
  void StartProgram(int n, double nowS, const char *why);
  void StopProgram(const char *why);
  bool PlayType(FBCmType type, const FBCmProgramType &p, Player &pl, const fb_fdm_state &st,
                double simTimeS);
  void Eject(FBCmType type, const fb_fdm_state &st, double simTimeS);
  void ServiceAutomatic(const FBState &state, double simTimeS);
  bool Low(FBCmType type) const;

  FBCmdsMode Mode_ = FBCmdsMode::Off;
  FBCmdsStatus Status_ = FBCmdsStatus::NoGo;   /* what Run() published, for the telemetry row */
  int  Selected_ = 1;
  int  Running_ = 0;            /* program number currently playing, 0 = none */
  bool Consent_ = false;
  bool AwaitingConsent_ = false;   /* SEMI: a threat is up and the prompt is out */

  int  Chaff_ = 0, Flare_ = 0;
  int  BingoChaff_ = 5, BingoFlare_ = 5;
  int  ChaffOut_ = 0, FlareOut_ = 0;   /* dispensed this sortie */

  Player ChaffPlayer_, FlarePlayer_;

  FBChaffCloud Clouds_[kMaxChaffClouds];
  int  NextCloud_ = 0;          /* ring: the freshest kMaxChaffClouds cartridges */
  int  ActiveClouds_ = 0;
  int  BlockStatus_ = 0;
};

} // namespace FlightBox
#endif
