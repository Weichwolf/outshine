#include "MvtTransportSourceFactsInternal.h"
#include "OsmField.h"
#include "StreetField.h"
#include "Wayfinding.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <span>
#include <stop_token>
#include <string>
#include <utility>

namespace outshine::Generators::Osm::MvtTransport {
std::expected<SourceFactCapture, std::string>
CaptureSourceFacts(const Sources &sources,
                   std::span<const Segment> segments,
                   const Path::Network &network,
                   const OsmField &field,
                   const StreetField &streets,
                   std::shared_ptr<const World::TransportNetworkSnapshot> snapshot,
                   std::shared_ptr<const MvtTransportNodeGroups> groups,
                   size_t bytesMost,
                   const std::stop_token &stop) {
  if (bytesMost == 0) { return SourceFactCapture{}; }
  if (!groups) {
    return SourceFactCapture{.State = MvtTransportSourceFactState::MissingNodeGroups, .Data = {}};
  }
  const SourceFactInput input{.SourceRows = sources,
                              .Segments = segments,
                              .Network = network,
                              .Field = field,
                              .Streets = streets};
  const bool fits = groups->Groups.size() <= std::numeric_limits<uint32_t>::max() &&
                    segments.size() <= std::numeric_limits<uint32_t>::max() / 2 &&
                    FitsSourceFacts(input, groups->Groups.size(), bytesMost, stop);
  if (stop.stop_requested()) { return std::unexpected("MVT transport normalization cancelled"); }
  if (!fits) {
    return SourceFactCapture{.State = MvtTransportSourceFactState::BudgetExceeded, .Data = {}};
  }
  auto facts = std::make_shared<MvtTransportSourceFacts>();
  facts->Network = std::move(snapshot);
  facts->NodeGroups = std::move(groups);
  facts->SourceGeneration = field.Generation();
  facts->SourceWayCount = streets.Ways().size();
  if (auto filled = FillSourceFacts(*facts, input, stop); !filled) {
    return std::unexpected(std::move(filled.error()));
  }
  BindSourceFacts(*facts);
  if (stop.stop_requested()) { return std::unexpected("MVT transport normalization cancelled"); }
  return SourceFactCapture{.State = MvtTransportSourceFactState::Captured,
                           .Data = std::move(facts)};
}
}
