#include "FBCountermeasureSystem.h"
#include "FBLog.h"
#include <cmath>

namespace FlightBox {

namespace {
const char *TypeName(FBCmType t) { return t == FBCmType::Chaff ? "chaff" : "flare"; }
} // namespace

FBCountermeasureSystem::FBCountermeasureSystem() {
  Chaff_ = 30;
  Flare_ = 30;   /* generic placeholder magazine; a module states its own (modules/f16/FBF16Cmds) */
  LoadGenericPrograms();
  for (FBChaffCloud &c : Clouds_) c = FBChaffCloud{};
}

/* A generic, deliberately unremarkable table: one chaff-only program, one mixed, one flare-only, one
 * long chaff pattern, the slap program and the bypass pair. Every figure is inside the ALE-47's
 * documented parameter ranges (core/FBCountermeasure.h) and none of them is a real jet's programming —
 * the same status FBRadarSystem::kGenericRangeNm carries. */
void FBCountermeasureSystem::LoadGenericPrograms() {
  FBCmProgram p{};
  p.Chaff = {2, 0.10, 2, 1.00};
  Programs_[0] = p;

  p = FBCmProgram{};
  p.Chaff = {2, 0.10, 2, 1.00};
  p.Flare = {1, 0.10, 2, 1.00};
  Programs_[1] = p;

  p = FBCmProgram{};
  p.Flare = {2, 0.10, 2, 1.00};
  Programs_[2] = p;

  p = FBCmProgram{};
  p.Chaff = {2, 0.10, 4, 4.00};
  Programs_[3] = p;

  p = FBCmProgram{};
  p.Chaff = {1, 0.10, 1, 1.00};
  p.Flare = {1, 0.10, 1, 1.00};
  Programs_[4] = p;

  /* BYP (doc/f16/defence-rwr-cm.md §2.2): exactly one chaff and one flare. */
  p = FBCmProgram{};
  p.Chaff = {1, 0.10, 1, 1.00};
  p.Flare = {1, 0.10, 1, 1.00};
  Programs_[5] = p;
}

/* [SET] — the source states that the system picks "the appropriate Automatic program per threat" and
 * never which, so the doctrine is ours: a radar merely TRACKING you gets the economical pattern (a
 * couple of clouds, repeated slowly, enough to break a lock without emptying the magazine); a radar
 * GUIDING a missile, or a seeker already looking at you, gets the dense one. */
int FBCountermeasureSystem::AutomaticProgram(FBRwrThreatMode worst) const {
  return worst == FBRwrThreatMode::Missile ? 1 : 4;
}

void FBCountermeasureSystem::SetMode(FBCmdsMode m) {
  if (Mode_ == m) return;
  Mode_ = m;
  /* "Consent defaults to granted every time the knob is turned to AUTO" (doc/f16/defence-rwr-cm.md
   * §2.2) — SEMI's consent is per dispense and therefore starts ungranted. */
  Consent_ = m == FBCmdsMode::Auto;
  AwaitingConsent_ = false;
  if (m == FBCmdsMode::Off || m == FBCmdsMode::Stby) StopProgram("mode");
}

bool FBCountermeasureSystem::SelectProgram(int n) {
  if (n < 1 || n > kProgramCount) return false;
  Selected_ = n;
  return true;
}

bool FBCountermeasureSystem::InstallProgram(int n, const FBCmProgram &p) {
  if (n < 1 || n > kProgramCount || !p.Valid()) return false;
  Programs_[n - 1] = p;
  return true;
}

bool FBCountermeasureSystem::SetProgram(int n, const FBCmProgram &p) {
  if (Mode_ != FBCmdsMode::Stby) return false;   /* §2.2: reprogramming is a STBY-only action */
  return InstallProgram(n, p);
}

const FBCmProgram &FBCountermeasureSystem::Program(int n) const {
  static const FBCmProgram kNone{};
  if (n < 1 || n > kProgramCount) return kNone;
  return Programs_[n - 1];
}

void FBCountermeasureSystem::SetLoadout(int chaff, int flare) {
  Chaff_ = chaff < 0 ? 0 : chaff;
  Flare_ = flare < 0 ? 0 : flare;
}

bool FBCountermeasureSystem::SetBingo(int chaff, int flare) {
  if (chaff < 0 || chaff > 99 || flare < 0 || flare > 99) return false;   /* §2.2's DED range */
  BingoChaff_ = chaff;
  BingoFlare_ = flare;
  return true;
}

void FBCountermeasureSystem::SetConsent(bool on) {
  Consent_ = on;
  if (!on) StopProgram("consent revoked");   /* CMS Right interrupts an in-progress program (§2.2) */
}

bool FBCountermeasureSystem::Low(FBCmType type) const {
  return type == FBCmType::Chaff ? Chaff_ <= BingoChaff_ : Flare_ <= BingoFlare_;
}

void FBCountermeasureSystem::StartProgram(int n, double nowS, const char *why) {
  const FBCmProgram &p = Programs_[n - 1];
  Running_ = n;
  ChaffPlayer_ = Player{};
  FlarePlayer_ = Player{};
  if (p.Chaff.Present()) {
    ChaffPlayer_.Running = true;
    ChaffPlayer_.SalvosLeft = p.Chaff.SalvoQty;
    ChaffPlayer_.BurstLeft = p.Chaff.BurstQty;
    ChaffPlayer_.NextS = nowS;
  }
  if (p.Flare.Present()) {
    FlarePlayer_.Running = true;
    FlarePlayer_.SalvosLeft = p.Flare.SalvoQty;
    FlarePlayer_.BurstLeft = p.Flare.BurstQty;
    FlarePlayer_.NextS = nowS;
  }
  FBLog::Info("cmds", "PROGRAM_START", {{"program", n}, {"why", why},
      {"mode", FBCmdsModeStr(Mode_)}, {"chaff", p.Chaff.Cartridges()}, {"flare", p.Flare.Cartridges()}});
}

void FBCountermeasureSystem::StopProgram(const char *why) {
  if (!Running_) return;
  FBLog::Info("cmds", "PROGRAM_END", {{"program", Running_}, {"why", why},
      {"chaffLeft", Chaff_}, {"flareLeft", Flare_}});
  Running_ = 0;
  ChaffPlayer_ = Player{};
  FlarePlayer_ = Player{};
}

/* One cartridge leaves the aircraft. Chaff becomes a cloud at the position the aircraft is at right now
 * and with no velocity of its own (core/FBCountermeasure.h): the ring keeps the freshest ones, which is
 * also the right set to keep, because an old cloud is a thin one. */
void FBCountermeasureSystem::Eject(FBCmType type, const fb_fdm_state &st, double simTimeS) {
  if (type == FBCmType::Chaff) {
    Chaff_--;
    ChaffOut_++;
    FBChaffCloud &c = Clouds_[NextCloud_];
    NextCloud_ = (NextCloud_ + 1) % kMaxChaffClouds;
    c.Active = true;
    c.LatDeg = st.lat;
    c.LonDeg = st.lon;
    c.AltM = st.elev;
    c.BloomS = simTimeS;
  } else {
    Flare_--;
    FlareOut_++;
  }
}

bool FBCountermeasureSystem::PlayType(FBCmType type, const FBCmProgramType &p, Player &pl,
                                      const fb_fdm_state &st, double simTimeS) {
  if (!pl.Running) return false;
  int fired = 0;
  /* Absolute-time schedule with a catch-up guard, the same shape as the radar's antenna frame grid: a
   * burst interval shorter than the slot's cadence still ejects the right number of cartridges at the
   * right times instead of being stretched to the tick rate. */
  int guard = 0;
  while (pl.Running && simTimeS >= pl.NextS && guard++ < 128) {
    int left = type == FBCmType::Chaff ? Chaff_ : Flare_;
    if (left <= 0) {
      FBLog::Warn("cmds", "MAGAZINE_EMPTY", {{"type", TypeName(type)}, {"program", Running_}});
      pl.Running = false;
      break;
    }
    Eject(type, st, simTimeS);
    fired++;
    pl.BurstLeft--;
    if (pl.BurstLeft > 0) {
      pl.NextS += p.BurstIntervalS;
      continue;
    }
    pl.SalvosLeft--;
    if (pl.SalvosLeft <= 0) { pl.Running = false; break; }
    pl.BurstLeft = p.BurstQty;
    pl.NextS += p.SalvoIntervalS;
  }
  if (fired > 0)
    FBLog::Info("cmds", "SALVO", {{"type", TypeName(type)}, {"n", fired},
        {"left", type == FBCmType::Chaff ? Chaff_ : Flare_},
        {"lo", Low(type)}, {"program", Running_}});
  return pl.Running;
}

bool FBCountermeasureSystem::Dispense(int program, double nowS, FBCommandOutcome &outcome,
                                      FBCommandReason &reason) {
  /* The knob physically locks the dispenser out in OFF/STBY — a hardware precedence, the same class of
   * refusal as a master-arm interlock (doc/f16/controls-commands.md §6.2). */
  if (Mode_ == FBCmdsMode::Off || Mode_ == FBCmdsMode::Stby) {
    outcome = FBCommandOutcome::Rejected;
    reason = FBCommandReason::HardwarePrecedence;
    return false;
  }
  /* BYP overrides the selection entirely: one chaff, one flare, and programs 1-6 are unavailable. */
  int n = Mode_ == FBCmdsMode::Byp ? kBypassProgram : (program > 0 ? program : Selected_);
  if (n < 1 || n > kProgramCount) {
    outcome = FBCommandOutcome::Rejected;
    reason = FBCommandReason::OutOfRange;
    return false;
  }
  const FBCmProgram &p = Programs_[n - 1];
  if (p.Empty()) {
    outcome = FBCommandOutcome::Rejected;
    reason = FBCommandReason::OutOfContext;   /* a program with both types zeroed dispenses nothing */
    return false;
  }
  bool haveAny = (p.Chaff.Present() && Chaff_ > 0) || (p.Flare.Present() && Flare_ > 0);
  if (!haveAny) {
    outcome = FBCommandOutcome::Rejected;
    reason = FBCommandReason::Depleted;
    return false;
  }
  StartProgram(n, nowS, Running_ ? "manual restart" : "manual");
  return true;
}

/* SEMI/AUTO: the system's own trigger, and it fires at the WARNING rather than at the truth (class
 * banner) — no threat on the RWR block means no automatic dispense, whatever is actually out there. */
void FBCountermeasureSystem::ServiceAutomatic(const FBState &state, double simTimeS) {
  if (Mode_ != FBCmdsMode::Semi && Mode_ != FBCmdsMode::Auto) { AwaitingConsent_ = false; return; }
  const FBRwrBlock &r = state.Rwr;
  bool threatened = r.H.Readable() && r.ThreatCount > 0 &&
                    r.Threats[0].Mode != FBRwrThreatMode::Search;
  if (!threatened) { AwaitingConsent_ = false; return; }
  if (Running_) return;
  /* §2.2: automatic (and only automatic) dispensing is withheld once chaff is at its BINGO quantity —
   * what is left is being kept for a shot the pilot decides to answer. */
  if (Low(FBCmType::Chaff)) { AwaitingConsent_ = false; return; }
  if (Mode_ == FBCmdsMode::Semi && !Consent_) { AwaitingConsent_ = true; return; }
  AwaitingConsent_ = false;
  StartProgram(AutomaticProgram(r.Threats[0].Mode), simTimeS,
               Mode_ == FBCmdsMode::Auto ? "auto" : "semi consent");
  if (Mode_ == FBCmdsMode::Semi) Consent_ = false;   /* consent is per dispense in SEMI */
}

void FBCountermeasureSystem::Run(FBState &state, const fb_fdm_state &st, double simTimeS) {
  ServiceAutomatic(state, simTimeS);

  if (Running_) {
    const FBCmProgram &p = Programs_[Running_ - 1];
    bool chaffOn = PlayType(FBCmType::Chaff, p.Chaff, ChaffPlayer_, st, simTimeS);
    bool flareOn = PlayType(FBCmType::Flare, p.Flare, FlarePlayer_, st, simTimeS);
    if (!chaffOn && !flareOn) StopProgram("complete");
  }

  ActiveClouds_ = 0;
  for (FBChaffCloud &c : Clouds_) {
    if (!c.Active) continue;
    if (FBChaffRcsNorm(simTimeS - c.BloomS) <= 0.0 && simTimeS - c.BloomS >= kChaffBloomS) c.Active = false;
    else ActiveClouds_++;
  }

  FBCmdsBlock &b = state.Cmds;
  b.Mode = Mode_;
  b.ProgramSelected = Selected_;
  b.ChaffRemaining = Chaff_;
  b.FlareRemaining = Flare_;
  b.ChaffLow = Low(FBCmType::Chaff);
  b.FlareLow = Low(FBCmType::Flare);
  b.Dispensing = Running_ != 0;
  b.ChaffDispensed = ChaffOut_;
  b.FlareDispensed = FlareOut_;
  b.ActiveClouds = ActiveClouds_;
  if (Mode_ == FBCmdsMode::Off) {
    Status_ = FBCmdsStatus::NoGo;
    b.H.Invalidate();   /* an unpowered dispenser reports nothing, which is not "nothing to report" */
  } else {
    Status_ = AwaitingConsent_ ? FBCmdsStatus::DispenseReady : FBCmdsStatus::Go;
    b.H.Publish(simTimeS);
  }
  b.Status = Status_;
  BlockStatus_ = (int)b.H.Status;
}

void FBCountermeasureSystem::DeclareTelemetry(FBTelemetrySchema &schema) const {
  /* FIRST column is this source's block validity — see systems/FBRwrSystem's schema for why it travels
   * here rather than in core/FBStateBusTelemetry's list. */
  schema.Add("blk_cmds");
  schema.Add("cm_mode");
  schema.Add("cm_status");
  schema.Add("cm_prog");
  schema.Add("cm_chaff");
  schema.Add("cm_flare");
  schema.Add("cm_lo");
  schema.Add("cm_disp");
  schema.Add("cm_out_chaff");
  schema.Add("cm_out_flare");
  schema.Add("cm_clouds");
}

void FBCountermeasureSystem::SampleTelemetry(FBTelemetryRow &row) const {
  row.Push(BlockStatus_);
  row.Push((int)Mode_);
  row.Push((int)Status_);
  row.Push(Selected_);
  row.Push(Chaff_);
  row.Push(Flare_);
  row.Push(Low(FBCmType::Chaff));
  row.Push(Running_ != 0);
  row.Push(ChaffOut_);
  row.Push(FlareOut_);
  row.Push(ActiveClouds_);
}

} // namespace FlightBox
