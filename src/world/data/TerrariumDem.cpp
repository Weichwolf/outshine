#include "TerrariumDem.h"

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace outshine::Data {

constexpr size_t kTypicalPayloadBytes = 60000;

namespace Says {
inline constexpr std::string_view kTile =
    "https://s3.amazonaws.com/elevation-tiles-prod/terrarium/{z}/{x}/{y}.png";
}

namespace {

[[nodiscard]] SourceDecl Declared(std::string revision,
                                  Rank order,
                                  AbsencePolicy absence,
                                  std::string dataset,
                                  std::string endpoint) {
  SourceDecl d;
  d.Id = dataset.empty() ? "terrarium.s3" : std::move(dataset);
  d.Version = 1;
  d.Revision = std::move(revision);
  d.Endpoint = std::move(endpoint);
  d.Kind = DataKind::Elevation;
  d.How = Scheme::TileZxy;
  d.Wire = WireFormat::TerrariumPng;
  d.Order = order;
  d.OnAbsent = absence;
  d.MinZoom = 0;

  d.MaxZoom = 15;
  d.AncestorFill = true;
  d.Keeps = Cacheability::Forever;
  d.Need = Necessity::Required;
  d.Latency = LatencyClass::Distant;

  d.TypicalPayloadBytes = kTypicalPayloadBytes;
  d.RetryBudget = 4;
  return d;
}

}

TerrariumDem::TerrariumDem(std::string revision,
                           Rank order,
                           AbsencePolicy absence,
                           std::string dataset,
                           const std::string &endpoint)
    : WebTileSource(Declared(std::move(revision), order, absence, std::move(dataset), endpoint),
                    endpoint.empty() ? std::string(Says::kTile) : endpoint) {}

bool TerrariumDem::CountsAbsent(int status) const noexcept {
  return status == kHttpForbidden || status == kHttpNotFound;
}

}
