#include "VectorTileSource.h"
#include "HttpOrigin.h"

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace outshine::Generators::Osm {

using namespace outshine::Data;

constexpr size_t kTypicalPayloadBytes = 80000;

namespace {

[[nodiscard]] SourceDecl Declared(std::string revision,
                                  Rank order,
                                  AbsencePolicy absence,
                                  std::string dataset,
                                  std::string endpoint,
                                  MvtSchema schema) {
  SourceDecl d;
  d.Id = std::move(dataset);
  d.Version = schema == MvtSchema::Shortbread ? 1 : 2;
  d.Schema = schema == MvtSchema::Shortbread ? "shortbread" : "openmaptiles";
  d.Revision = std::move(revision);
  d.Endpoint = std::move(endpoint);
  d.Kind = DataKind::VectorMap;
  d.How = Scheme::TileZxy;
  d.Wire = WireFormat::MapboxVectorTile;
  d.Order = order;
  d.OnAbsent = absence;
  d.MinZoom = 0;
  d.MaxZoom = 14;
  d.AncestorFill = false;
  d.Keeps = Cacheability::Forever;
  d.Need = Necessity::Required;
  d.Latency = LatencyClass::Distant;

  d.TypicalPayloadBytes = kTypicalPayloadBytes;
  d.RetryBudget = 4;
  return d;
}

}

VectorTileSource::VectorTileSource(std::string revision,
                                   Rank order,
                                   AbsencePolicy absence,
                                   std::string dataset,
                                   const std::string &endpoint,
                                   MvtSchema schema)
    : WebTileSource(
          Declared(std::move(revision), order, absence, std::move(dataset), endpoint, schema),
          endpoint) {
  const auto &declared = Declaration();
  const auto origin = HttpOrigin(endpoint);
  if (declared.Id == "openfreemap.openmaptiles" ||
      origin.starts_with("https://tiles.openfreemap.org:") ||
      origin.starts_with("http://tiles.openfreemap.org:") ||
      origin.starts_with("https://tiles.openfreemap.com:") ||
      origin.starts_with("http://tiles.openfreemap.com:")) {
    ExpectedRelease_ = "";
    const auto path = std::string_view(endpoint).substr(0, endpoint.find_first_of("?#"));
    const auto suffix = "/" + declared.Revision + "/{z}/{x}/{y}.pbf";
    if (!path.ends_with(suffix)) { return; }
    const auto prefix = path.substr(0, path.size() - suffix.size());
    const auto slash = prefix.rfind('/');
    if (slash == std::string_view::npos || slash + 1 == prefix.size()) { return; }
    ExpectedRelease_ =
        "specific PBF " + std::string(prefix.substr(slash + 1)) + " " + declared.Revision;
    RevisionEndpoint_ = true;
  }
}

bool VectorTileSource::AcceptsIdentity(const Wire::Response &response) const noexcept {
  if (!ExpectedRelease_) { return true; }
  if (!RevisionEndpoint_) { return false; }
  const auto field = SingleResponseHeader(response.Headers, "x-ofm-debug");
  return field &&
         (*field == *ExpectedRelease_ || (*field == "empty tile" && response.Body.empty()));
}

}
