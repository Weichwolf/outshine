/* The ONE path from a pilot's intent to an avionics box, and the receipt back. Owned by the module; the
 * pilot POSTS, the module hands each due command to its owning system in that system's tick, and that
 * system COMPLETES it. Fixed capacity, no allocation.
 * Three rules before any system sees a command: LATENCY (nothing arrives faster than a hand moves),
 * OCCUPANCY (one head, one switch at a time -> ChannelBusy), and the MANOEUVRE GATE (above kDedMaxG the
 * pilot is flying, not typing -> SequencePrecondition). Everything else is the owning system's answer.
 * Also the command stream's RECORDER (FBTelemetrySource "cmd" + FBLog "cmd" events).
 * doc/core.md, Abschnitt 2.6. */
#ifndef FBCOMMANDBUS_H
#define FBCOMMANDBUS_H

#include "FBAvionicsCommand.h"
#include "FBBodyAngle.h"
#include "FBTelemetry.h"

namespace FlightBox {

class FBCommandBus : public FBTelemetrySource {
public:
  /* The documented short/long press discriminator (doc/modules/f16/controls-commands.md §5). */
  static constexpr double kHotasLatencyS = 0.5;
  /* FlightBox's own numbers, derived not quoted — the guides give neither. Derivations:
   * doc/core.md, Abschnitt 2.6. */
  static constexpr double kDedLatencyS = 4.0;
  static constexpr double kDedMaxG = 1.5;
  /* The one HOTAS action whose latency is NOT a press duration: the barrels' spool-up is modelled where
   * it belongs (FBGun.h's SpoolUpS), so what is left here is the FINGER. The SPACING between two
   * squeezes stays kHotasLatencyS. */
  static constexpr double kTriggerLatencyS = 0.1;

  /* How long an action of this target's class takes to reach the box that answers it. */
  static double LatencyS(FBCommandTarget t) {
    if (t == FBCommandTarget::GunTrigger) return kTriggerLatencyS;
    return FBCommandClassOf(t) == FBCommandClass::Ded ? kDedLatencyS : kHotasLatencyS;
  }

  static constexpr int kMaxPending = 8;

  ~FBCommandBus() override = default;

  /* The pilot's one verb: Pending if the command entered the queue, final Rejected if the bus itself
   * refused it. */
  FBCommandAck Post(FBCommandTarget target, double value, double nowS);

  /* WHERE THE ANTENNA IS POINTED, and the ONE door to it: an antenna command is BODY-referenced, so it
   * takes core/FBBodyAngle and not a double. RadarSlewAz/El are named here and nowhere else in the tree
   * (tools/verify_layers.py counts the files that may), which is what turns "a world angle must be
   * converted first" from a comment into a thing that cannot be written. */
  FBCommandAck PostAntennaAz(FBBodyAngle az, double nowS) {
    return Post(FBCommandTarget::RadarSlewAz, az.Deg(), nowS);
  }
  FBCommandAck PostAntennaEl(FBBodyAngle el, double nowS) {
    return Post(FBCommandTarget::RadarSlewEl, el.Deg(), nowS);
  }

  /* IS THIS SWITCH FREE AGAIN — the OCCUPANCY rule above, asked instead of guessed. A hand that keeps
   * a switch held has to know when it may throw it again, and the window is not derivable from the
   * outside: it runs kHotasLatencyS from the COMPLETION, and how long a completion takes is the owning
   * system's cadence. Guessing it costs one ChannelBusy rejection per repeat — a real refusal, but of a
   * command nobody meant to send. Changes no outcome: Post() applies the identical test. */
  bool SwitchReady(FBCommandTarget target, double nowS) const { return !SameSwitchBusy(target, nowS); }

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
  /* Last completion per target ordinal — the same-switch window; flat array = O(1), no allocation. */
  static constexpr int kTargetSlots = 32;
  static_assert((int)FBCommandTarget::MfdPageSelect < kTargetSlots, "widen kTargetSlots for new targets");
  double LastActionS_[kTargetSlots]{};
  bool   HaveLastAction_[kTargetSlots]{};

  FBCommandAck LastAck_{};
  int Issued_ = 0, Accepted_ = 0, Rejected_ = 0, Clamped_ = 0, Inhibited_ = 0;
};

} // namespace FlightBox
#endif
