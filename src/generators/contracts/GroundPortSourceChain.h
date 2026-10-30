#ifndef OUTSHINE_GENERATORS_CONTRACTS_GROUNDPORTSOURCECHAIN_H
#define OUTSHINE_GENERATORS_CONTRACTS_GROUNDPORTSOURCECHAIN_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace outshine::World {
class TransportNetworkSnapshot;
}

namespace outshine::Generators {
struct GroundPortChainWork {
  size_t SelectedSegments = 0;
  size_t IncidenceVisits = 0;
  size_t NodesVisited = 0;
  size_t ProfileSegmentsChecked = 0;
  size_t OrderedSegments = 0;
  size_t ContainerCapacityBytes = 0;
};

struct GroundPortSourceChain {
  std::shared_ptr<const World::TransportNetworkSnapshot> Source;
  std::vector<uint32_t> Nodes;
  std::vector<uint32_t> SourceEdges;
  double WidthM = 0;
  double MaximumGradient = 0;
  GroundPortChainWork Work;
};

}
#endif
