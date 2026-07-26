/* FlightBox — FBCommandBus: the ONE path from a pilot's intent to an avionics box, and the receipt
 * coming back. Owned by the module (like the state bus it mirrors); the pilot POSTS, the module hands
 * each due command to the system that owns it in that system's own tick, and that system COMPLETES it.
 * Fixed capacity, no allocation, no ownership of anything.
 *
 * WHAT IT ENFORCES BY ITSELF, before any system sees a command:
 *   - LATENCY. A command is not consumable before IssuedS + its class latency (core/
 *     FBAvionicsCommand.h): 0.5 s for a HOTAS switch action, kDedLatencyS for a DED field edit. Nothing
 *     an AI does can arrive faster than a hand can move.
 *   - OCCUPANCY. One DED edit at a time (a pilot has one head), and the same HOTAS switch cannot be
 *     worked twice inside one press-duration window. A second one is Rejected(ChannelBusy) — the
 *     documented ~0.5-1 s floor between two different commands on the same switch (§5).
 *   - THE MANOEUVRE GATE. A DED edit is head-down, hands-off work; above kDedMaxG the pilot is flying
 *     the jet, not typing. Rejected(SequencePrecondition). This is the rule that keeps an AI from
 *     entering steerpoints in a knife fight, which is precisely what the two-class split exists for,
 *     and it is FlightBox's own number (§5 gives no g figure — see kDedMaxG).
 * Everything else — does this value make sense, is this box even fitted — is the OWNING SYSTEM's
 * answer, because that is where the knowledge is.
 *
 * IT IS ALSO THE COMMAND STREAM'S RECORDER (FBTelemetrySource "cmd" + FBLog "cmd" events), so a gym run
 * shows WHAT the AI operated long before there is a display to watch it on. */
#ifndef FBCOMMANDBUS_H
#define FBCOMMANDBUS_H

#include "FBAvionicsCommand.h"
#include "FBTelemetry.h"

namespace FlightBox {

class FBCommandBus : public FBTelemetrySource {
public:
  /* The documented short/long press discriminator (doc/f16/controls-commands.md §5): the avionics
   * itself uses 0.5 s to tell two commands on one switch apart, so that is the floor for a HOTAS
   * action's effect and for re-using the same switch. */
  static constexpr double kHotasLatencyS = 0.5;
  /* FlightBox's own number, derived not quoted: §5 says a DED field write is "realistically several
   * seconds per field (select field, type digits, press ENTR)" and gives no figure. Four seconds is the
   * middle of "several" and — the point — an order of magnitude above the HOTAS class, which is the
   * property the model needs. */
  static constexpr double kDedLatencyS = 4.0;
  /* FlightBox's own number again: no guide states a g limit for data entry. 1.5 g is a shade above
   * level flight — enough that a gentle cruise turn does not lock the DED, low enough that anything
   * recognisable as manoeuvring does. */
  static constexpr double kDedMaxG = 1.5;

  static constexpr int kMaxPending = 8;

  ~FBCommandBus() override = default;

  /* The pilot's one verb. Returns the receipt as it stands right now: Pending if the command entered
   * the queue, or a final Rejected if the bus itself refused it (see the class banner). */
  FBCommandAck Post(FBCommandTarget target, double value, double nowS);

  /* The module's side: hand out the next command of `group` whose latency has elapsed. Returns false
   * when there is nothing due. */
  bool TakeDue(FBCommandGroup group, double nowS, FBAvionicsCommand &out);
  /* ... and the owning system's answer. */
  void Complete(const FBAvionicsCommand &cmd, FBCommandOutcome outcome, FBCommandReason reason,
                double nowS);

  /* The manoeuvre state the DED gate reads, published by the module from the air-data block. */
  void SetLoadFactor(double g) { LoadFactorG_ = g; }

  int PendingCount() const { return Count_; }
  const FBCommandAck &LastAck() const { return LastAck_; }
  int IssuedCount() const { return Issued_; }
  int AcceptedCount() const { return Accepted_; }
  int RejectedCount() const { return Rejected_; }
  int ClampedCount() const { return Clamped_; }
  int InhibitedCount() const { return Inhibited_; }

  const char *TelemetryName() const override { return "cmd"; }
  void DeclareTelemetry(FBTelemetrySchema &schema) const override;
  void SampleTelemetry(FBTelemetryRow &row) const override;

private:
  FBCommandAck Reject(FBCommandTarget target, double value, double nowS, FBCommandReason reason);
  bool SameSwitchBusy(FBCommandTarget target, double nowS) const;
  bool DedChannelBusy() const;

  FBAvionicsCommand Queue_[kMaxPending]{};
  int Count_ = 0;
  uint32_t NextSeq_ = 1;
  double LoadFactorG_ = 1.0;
  /* Last completion time per target ordinal is what the same-switch window is measured against; a flat
   * array keeps it allocation-free and O(1). +1 for the None slot. */
  static constexpr int kTargetSlots = 32;
  static_assert((int)FBCommandTarget::Designate < kTargetSlots, "widen kTargetSlots for new targets");
  double LastActionS_[kTargetSlots]{};
  bool   HaveLastAction_[kTargetSlots]{};

  FBCommandAck LastAck_{};
  int Issued_ = 0, Accepted_ = 0, Rejected_ = 0, Clamped_ = 0, Inhibited_ = 0;
};

} // namespace FlightBox
#endif
