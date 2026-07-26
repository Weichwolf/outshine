#include "FBCommandBus.h"
#include "FBLog.h"
#include <cmath>

namespace FlightBox {

const char *FBCommandTargetStr(FBCommandTarget t) {
  switch (t) {
    case FBCommandTarget::None: return "none";
    case FBCommandTarget::RadarMode: return "radar_mode";
    case FBCommandTarget::RadarRangeNm: return "radar_range_nm";
    case FBCommandTarget::RadarSlewAz: return "radar_slew_az";
    case FBCommandTarget::RadarSlewEl: return "radar_slew_el";
    case FBCommandTarget::IffTransponder: return "iff_xpdr";
    case FBCommandTarget::IffInterrogator: return "iff_interrogator";
    case FBCommandTarget::DatalinkPower: return "datalink_power";
    case FBCommandTarget::DatalinkTransmit: return "datalink_xmt";
    case FBCommandTarget::DatalinkFilter: return "datalink_filter";
    case FBCommandTarget::DatalinkRangeNm: return "datalink_range_nm";
    case FBCommandTarget::MasterMode: return "master_mode";
    case FBCommandTarget::MasterArm: return "master_arm";
    case FBCommandTarget::AlowFt: return "alow_ft";
    case FBCommandTarget::BingoLbs: return "bingo_lbs";
    case FBCommandTarget::SteerpointNum: return "steerpoint";
    case FBCommandTarget::WeaponSelect: return "weapon_select";
    case FBCommandTarget::Designate: return "designate";
    case FBCommandTarget::StationSelect: return "station_select";
    case FBCommandTarget::WeaponRelease: return "weapon_release";
  }
  return "?";
}

/* HOTAS = one press or one switch throw the hand already rests on; DED = a field edit through the
 * propose/commit cycle (doc/f16/controls-commands.md §1.2 vs §3). Range/threshold/steerpoint entries
 * are typed; modes, switches and cursor slews are not. */
FBCommandClass FBCommandClassOf(FBCommandTarget t) {
  switch (t) {
    case FBCommandTarget::RadarRangeNm:
    case FBCommandTarget::DatalinkRangeNm:
    case FBCommandTarget::AlowFt:
    case FBCommandTarget::BingoLbs:
    case FBCommandTarget::SteerpointNum:
      return FBCommandClass::Ded;
    default:
      return FBCommandClass::Hotas;
  }
}

FBCommandGroup FBCommandGroupOf(FBCommandTarget t) {
  switch (t) {
    case FBCommandTarget::RadarMode:
    case FBCommandTarget::RadarRangeNm:
    case FBCommandTarget::RadarSlewAz:
    case FBCommandTarget::RadarSlewEl:
    case FBCommandTarget::IffTransponder:
    case FBCommandTarget::IffInterrogator:
      return FBCommandGroup::Sensors;
    case FBCommandTarget::DatalinkPower:
    case FBCommandTarget::DatalinkTransmit:
    case FBCommandTarget::DatalinkFilter:
    case FBCommandTarget::DatalinkRangeNm:
      return FBCommandGroup::Comms;
    /* The SMS owns both, and it is cycled with the module's weapons slot — a pickle is answered by the
     * box that actually lets go of the store, at that box's own rate. */
    case FBCommandTarget::StationSelect:
    case FBCommandTarget::WeaponRelease:
      return FBCommandGroup::Stores;
    default:
      return FBCommandGroup::Avionics;
  }
}

const char *FBCommandOutcomeStr(FBCommandOutcome o) {
  switch (o) {
    case FBCommandOutcome::Pending: return "pending";
    case FBCommandOutcome::Accepted: return "accepted";
    case FBCommandOutcome::Clamped: return "clamped";
    case FBCommandOutcome::Inhibited: return "inhibited";
    case FBCommandOutcome::Rejected: return "rejected";
  }
  return "?";
}

const char *FBCommandReasonStr(FBCommandReason r) {
  switch (r) {
    case FBCommandReason::None: return "none";
    case FBCommandReason::PilotReject: return "pilot_reject";
    case FBCommandReason::HardwarePrecedence: return "hardware_precedence";
    case FBCommandReason::SequencePrecondition: return "sequence_precondition";
    case FBCommandReason::EffectPrecondition: return "effect_precondition";
    case FBCommandReason::OutOfContext: return "out_of_context";
    case FBCommandReason::NotImplemented: return "not_implemented";
    case FBCommandReason::SoftFailure: return "soft_failure";
    case FBCommandReason::ValueClamped: return "value_clamped";
    case FBCommandReason::OutOfRange: return "out_of_range";
    case FBCommandReason::ChannelBusy: return "channel_busy";
  }
  return "?";
}

FBCommandAck FBCommandBus::Reject(FBCommandTarget target, double value, double nowS,
                                  FBCommandReason reason) {
  FBCommandAck a{};
  a.Seq = NextSeq_++;
  a.Target = target;
  a.Value = value;
  a.Outcome = FBCommandOutcome::Rejected;
  a.Reason = reason;
  a.CompletedS = nowS;
  Issued_++;
  Rejected_++;
  LastAck_ = a;
  FBLog::Info("cmd", "CMD_REJECT", {{"seq", (int)a.Seq}, {"target", FBCommandTargetStr(target)},
      {"value", value}, {"reason", FBCommandReasonStr(reason)}});
  return a;
}

bool FBCommandBus::SameSwitchBusy(FBCommandTarget target, double nowS) const {
  int slot = (int)target;
  if (slot < 0 || slot >= kTargetSlots || !HaveLastAction_[slot]) return false;
  return nowS - LastActionS_[slot] < kHotasLatencyS;
}

bool FBCommandBus::DedChannelBusy() const {
  for (int i = 0; i < Count_; i++)
    if (FBCommandClassOf(Queue_[i].Target) == FBCommandClass::Ded) return true;
  return false;
}

FBCommandAck FBCommandBus::Post(FBCommandTarget target, double value, double nowS) {
  if (target == FBCommandTarget::None) return Reject(target, value, nowS, FBCommandReason::OutOfContext);
  FBCommandClass cls = FBCommandClassOf(target);

  /* The manoeuvre gate: head-down work needs a jet that is not being flown hard (class banner). */
  if (cls == FBCommandClass::Ded && std::fabs(LoadFactorG_) > kDedMaxG)
    return Reject(target, value, nowS, FBCommandReason::SequencePrecondition);
  if (cls == FBCommandClass::Ded && DedChannelBusy())
    return Reject(target, value, nowS, FBCommandReason::ChannelBusy);
  if (cls == FBCommandClass::Hotas && SameSwitchBusy(target, nowS))
    return Reject(target, value, nowS, FBCommandReason::ChannelBusy);
  if (Count_ >= kMaxPending) return Reject(target, value, nowS, FBCommandReason::ChannelBusy);

  FBAvionicsCommand &c = Queue_[Count_++];
  c.Seq = NextSeq_++;
  c.Target = target;
  c.Value = value;
  c.IssuedS = nowS;
  c.DueS = nowS + (cls == FBCommandClass::Ded ? kDedLatencyS : kHotasLatencyS);
  Issued_++;

  FBLog::Info("cmd", "CMD_ISSUE", {{"seq", (int)c.Seq}, {"target", FBCommandTargetStr(target)},
      {"value", value}, {"class", cls == FBCommandClass::Ded ? "ded" : "hotas"}, {"dueS", c.DueS}});

  FBCommandAck a{};
  a.Seq = c.Seq; a.Target = target; a.Value = value;
  a.Outcome = FBCommandOutcome::Pending;
  return a;
}

bool FBCommandBus::TakeDue(FBCommandGroup group, double nowS, FBAvionicsCommand &out) {
  for (int i = 0; i < Count_; i++) {
    if (FBCommandGroupOf(Queue_[i].Target) != group || Queue_[i].DueS > nowS) continue;
    out = Queue_[i];
    for (int k = i; k + 1 < Count_; k++) Queue_[k] = Queue_[k + 1];   /* order-preserving removal */
    Count_--;
    return true;
  }
  return false;
}

void FBCommandBus::Complete(const FBAvionicsCommand &cmd, FBCommandOutcome outcome,
                            FBCommandReason reason, double nowS) {
  FBCommandAck a{};
  a.Seq = cmd.Seq; a.Target = cmd.Target; a.Value = cmd.Value;
  a.Outcome = outcome; a.Reason = reason; a.CompletedS = nowS;
  LastAck_ = a;

  int slot = (int)cmd.Target;
  if (slot >= 0 && slot < kTargetSlots) { LastActionS_[slot] = nowS; HaveLastAction_[slot] = true; }

  switch (outcome) {
    case FBCommandOutcome::Accepted: Accepted_++; break;
    case FBCommandOutcome::Clamped: Accepted_++; Clamped_++; break;
    case FBCommandOutcome::Inhibited: Accepted_++; Inhibited_++; break;
    case FBCommandOutcome::Rejected: Rejected_++; break;
    case FBCommandOutcome::Pending: break;
  }
  FBLog::Info("cmd", "CMD_ACK", {{"seq", (int)a.Seq}, {"target", FBCommandTargetStr(a.Target)},
      {"value", a.Value}, {"outcome", FBCommandOutcomeStr(outcome)},
      {"reason", FBCommandReasonStr(reason)}, {"latencyS", nowS - cmd.IssuedS}});
}

void FBCommandBus::DeclareTelemetry(FBTelemetrySchema &schema) const {
  schema.Add("cmd_issued");
  schema.Add("cmd_accepted");
  schema.Add("cmd_rejected");
  schema.Add("cmd_clamped");
  schema.Add("cmd_inhibited");
  schema.Add("cmd_pending");
  schema.Add("cmd_last");
  schema.Add("cmd_last_outcome");
  schema.Add("cmd_last_reason");
}

void FBCommandBus::SampleTelemetry(FBTelemetryRow &row) const {
  row.Push(Issued_);
  row.Push(Accepted_);
  row.Push(Rejected_);
  row.Push(Clamped_);
  row.Push(Inhibited_);
  row.Push(Count_);
  row.Push(std::string(FBCommandTargetStr(LastAck_.Target)));
  row.Push(std::string(FBCommandOutcomeStr(LastAck_.Outcome)));
  row.Push(std::string(FBCommandReasonStr(LastAck_.Reason)));
}

} // namespace FlightBox
