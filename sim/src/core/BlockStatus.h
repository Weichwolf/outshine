/* The validity head every avionics OUTPUT block carries. THREE states, not two, and the third is
 * DOCUMENTED rather than invented: several CRUS-page fields freeze-at-last-value once the gear is down
 * — a failed system and a deliberately frozen one are different facts and a consumer reacts to them
 * differently. StampS is the last real UPDATE in both Valid and Held; Hold() never moves it, which is
 * what makes "how stale is this" answerable. */
#ifndef BLOCKSTATUS_H
#define BLOCKSTATUS_H

#include <cstdint>

namespace outshine {

enum class BlockStatus : uint8_t { Invalid = 0, Valid = 1, Held = 2 };

inline const char *BlockStatusStr(BlockStatus s) {
  switch (s) {
    case BlockStatus::Invalid: return "invalid";
    case BlockStatus::Valid: return "valid";
    case BlockStatus::Held: return "held";
  }
  return "?";
}

struct BlockHeader {
  double StampS = 0.0;                                 /* sim time of the last real update */
  BlockStatus Status = BlockStatus::Invalid;

  bool IsValid() const { return Status == BlockStatus::Valid; }
  bool IsHeld() const { return Status == BlockStatus::Held; }
  /* May I read the numbers at all? Valid AND Held say yes; AgeS answers whether they are current. */
  bool Readable() const { return Status != BlockStatus::Invalid; }
  double AgeS(double nowS) const { return nowS - StampS; }

  void Publish(double nowS) { StampS = nowS; Status = BlockStatus::Valid; }
  /* A block that was never published has nothing to hold, so it stays Invalid. */
  void Hold() { if (Status == BlockStatus::Valid) Status = BlockStatus::Held; }
  void Invalidate() { Status = BlockStatus::Invalid; }
};

} // namespace outshine
#endif
