/* FlightBox — FBBlockStatus/FBBlockHeader: the validity head every avionics OUTPUT block carries
 * (core/FBAvionicsBlocks.h). The semantics come from a multiplex-bus jet, not the addressing: a
 * consumer must be able to tell "this number is current", "this number is deliberately frozen" and
 * "this number means nothing" apart, because a display that keeps painting a dead sensor's last value
 * is the failure mode the whole scheme exists to prevent.
 *
 * THREE states, not two, and the third one is documented rather than invented: DCS/ED model several
 * CRUS-page computed fields as FREEZE-AT-LAST-VALUE once the gear is down — they stop updating, they do
 * not blank (doc/f16/controls-commands.md, "The DED's propose -> commit/reject protocol"). A failed
 * system and a deliberately frozen one are different facts and a consumer reacts to them differently
 * (blank the cue vs. keep showing the last good number), so they get different states:
 *   Invalid — no meaning. Never written, or its source system is off/failed. Consumers declutter.
 *   Valid   — written by its one writer at StampS, current as of then.
 *   Held    — the writer deliberately stopped updating it. The FIELDS still carry the last good
 *             values and StampS still says when they were taken, so age is answerable.
 * StampS is the timestamp of the last real UPDATE in both Valid and Held — Hold() never moves it,
 * which is exactly what makes "how stale is this held number" a question the reader can answer. */
#ifndef FBBLOCKSTATUS_H
#define FBBLOCKSTATUS_H

#include <cstdint>

namespace FlightBox {

enum class FBBlockStatus : uint8_t { Invalid = 0, Valid = 1, Held = 2 };

inline const char *FBBlockStatusStr(FBBlockStatus s) {
  switch (s) {
    case FBBlockStatus::Invalid: return "invalid";
    case FBBlockStatus::Valid: return "valid";
    case FBBlockStatus::Held: return "held";
  }
  return "?";
}

struct FBBlockHeader {
  double StampS = 0.0;                                 /* sim time of the last real update */
  FBBlockStatus Status = FBBlockStatus::Invalid;

  bool IsValid() const { return Status == FBBlockStatus::Valid; }
  bool IsHeld() const { return Status == FBBlockStatus::Held; }
  /* The consumer's usual question: may I read the numbers at all? Valid AND Held both say yes — the
   * difference is whether they are current, which AgeS answers. */
  bool Readable() const { return Status != FBBlockStatus::Invalid; }
  double AgeS(double nowS) const { return nowS - StampS; }

  void Publish(double nowS) { StampS = nowS; Status = FBBlockStatus::Valid; }
  /* Freeze: keep the values AND the stamp of the last real update. A block that was never published
   * has nothing to hold, so it stays Invalid. */
  void Hold() { if (Status == FBBlockStatus::Valid) Status = FBBlockStatus::Held; }
  void Invalidate() { Status = FBBlockStatus::Invalid; }
};

} // namespace FlightBox
#endif
