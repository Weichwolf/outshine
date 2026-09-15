#ifndef OUTSHINE_GENERATORS_FLORA_MODELLADDER_H
#define OUTSHINE_GENERATORS_FLORA_MODELLADDER_H

#include <cstddef>
#include <optional>

namespace outshine::ModelLadder {

constexpr int kLevels = 4;

constexpr float kCellPx = 256.0f;

[[nodiscard]] constexpr std::optional<float> RelativeDeviation(size_t level) {
  if (level >= static_cast<size_t>(kLevels)) { return std::nullopt; }
  return 1.0f / (kCellPx * static_cast<float>(1u << (static_cast<size_t>(kLevels) - level)));
}

}
#endif
