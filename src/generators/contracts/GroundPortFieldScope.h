#ifndef OUTSHINE_GENERATORS_CONTRACTS_GROUNDPORTFIELDSCOPE_H
#define OUTSHINE_GENERATORS_CONTRACTS_GROUNDPORTFIELDSCOPE_H

#include "GroundPortSourceChain.h"
#include "TransportPlan.h"
#include "TransportTopology.h"
#include <array>
#include <cstdint>
#include <vector>

namespace outshine::Generators {
struct GroundPortFieldExit {
  uint32_t SourceGroup = 0;
  World::TransportEdgeId Ground;
  World::TransportEdgeId External;
  uint64_t ExternalNode = 0;
  uint8_t GroundEnd = 0;
  uint8_t ExternalEnd = 0;
  int32_t Layer = 0;
  bool Bridge = false;
  bool Tunnel = false;
  bool operator==(const GroundPortFieldExit &) const = default;
};

struct GroundPortFieldScopeInfo {
  TransportInputReference Input;
  std::array<uint8_t, 32> ContentBinding{};
  std::vector<GroundPortFieldExit> Exits;
  std::vector<TransportSourceEndTopology> Ends;
};

struct GroundPortFieldScope {
  GroundPortSourceChain Source;
  GroundPortFieldScopeInfo Info;
};
}
#endif
