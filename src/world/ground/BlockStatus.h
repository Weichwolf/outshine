#ifndef OUTSHINE_WORLD_GROUND_BLOCKSTATUS_H
#define OUTSHINE_WORLD_GROUND_BLOCKSTATUS_H

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
  double StampS = 0.0;
  BlockStatus Status = BlockStatus::Invalid;

  [[nodiscard]] bool IsValid() const { return Status == BlockStatus::Valid; }
  [[nodiscard]] bool IsHeld() const { return Status == BlockStatus::Held; }

  [[nodiscard]] bool Readable() const { return Status != BlockStatus::Invalid; }
  double AgeS(double nowS) const { return nowS - StampS; }

  void Publish(double nowS) { StampS = nowS; Status = BlockStatus::Valid; }

  void Hold() { if (Status == BlockStatus::Valid) Status = BlockStatus::Held; }
  void Invalidate() { Status = BlockStatus::Invalid; }
};

}
#endif
