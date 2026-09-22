#ifndef OUTSHINE_WORLD_NAVIGATION_TRANSPORTNETWORK_H
#define OUTSHINE_WORLD_NAVIGATION_TRANSPORTNETWORK_H

#include <cstddef>
#include <expected>
#include <memory>
#include <span>
#include <string>
#include <string_view>

#include "GroundStack.h"
#include "Wayfinding.h"

namespace outshine::World {

class TransportNetwork {
public:
  struct Built {
    std::shared_ptr<const Path::Network> Graph;
    size_t Ways = 0;
    size_t Nodes = 0;
    size_t Edges = 0;
    size_t Junctions = 0;
    Path::Network::Elevated Elevated;
    std::string Refusal;
    double LayMs = 0.0;
    double WeaveMs = 0.0;
    Path::Network::WeaveTimings WeavePhases;
    double CrossingsMs = 0.0;
    double ElevateMs = 0.0;
  };

  [[nodiscard]] static Built BuildOneShot(const Ground::GroundStack &stack);

private:
  [[nodiscard]] static std::expected<void, std::string_view>
  LayWays(const Ground::StreetField &ways, std::span<const double> points, Path::Network &graph);
};

}

#endif
