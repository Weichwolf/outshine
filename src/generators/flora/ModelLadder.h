#ifndef OUTSHINE_GENERATORS_FLORA_MODELLADDER_H
#define OUTSHINE_GENERATORS_FLORA_MODELLADDER_H

namespace outshine {

constexpr int kElementsPerSheet = 16;

namespace ModelLadder {

constexpr int kLevels = 4;

constexpr float kCellPx = 256.0f;

constexpr float Error(int k) {
  return 1.0f / (kCellPx * static_cast<float>(1u << static_cast<unsigned>(kLevels - k)));
}

} // namespace ModelLadder
} // namespace outshine
#endif
