#ifndef OUTSHINE_AUDIO_SIGNALGRAPH_H
#define OUTSHINE_AUDIO_SIGNALGRAPH_H

#include <cstddef>
#include <expected>
#include <span>
#include <string>
#include <vector>
#include <scenario/Scenario.h>

namespace outshine::Audio {

struct SignalGraphBudget {
  size_t Nodes = 1024;
  size_t Edges = 4096;
};

struct SignalGraph {
  std::vector<std::vector<size_t>> Inputs;
  std::vector<size_t> Order;

  [[nodiscard]] static std::expected<SignalGraph, std::string>
  Compile(std::span<const Scenario::Voice> voices, SignalGraphBudget &remaining);
};

}
#endif
