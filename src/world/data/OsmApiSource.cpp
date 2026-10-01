#include "OsmApiSource.h"

#include "OsmXmlReader.h"
#include "SourceProviderValidation.h"
#include <world/SourceProvider.h>

#include <format>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <world/data/GeoCellId.h>

namespace outshine::Data {
namespace {
constexpr int kHttpOk = 200;
constexpr int kHttpBadRequest = 400;
constexpr int kHttpNotFound = 404;
constexpr int kHttpRequestTimeout = 408;
constexpr int kHttpTooManyRequests = 429;
constexpr int kHttpServerError = 500;

std::string MapEndpoint(std::string_view endpoint, const SourceCoverage &bounds) {
  return std::format("{}/map?bbox={},{},{},{}",
                     endpoint,
                     bounds.WestDeg,
                     bounds.SouthDeg,
                     bounds.EastDeg,
                     bounds.NorthDeg);
}

std::optional<SourceCoverage> ApiCellBounds(const Address &at) noexcept {
  const auto cell = at.GeoCell();
  if (!cell) { return std::nullopt; }
  const auto bounds = cell->Bounds();
  if (!bounds || (bounds->EastDeg - bounds->WestDeg) * (bounds->NorthDeg - bounds->SouthDeg) >
                     kOsmApiMaximumAreaDeg2) {
    return std::nullopt;
  }
  return bounds;
}
}

std::expected<std::unique_ptr<OsmApiSource>, std::string>
OsmApiSource::Create(const SourceProvider &provider, uint32_t region) {
  if (provider.Kind != "osm" || provider.Endpoint.empty()) {
    return std::unexpected("an original OSM API source requires an osm endpoint declaration");
  }
  if (auto valid = ValidateSourceProviders(std::span(&provider, 1)); !valid) {
    return std::unexpected(std::move(valid.error()));
  }
  return std::unique_ptr<OsmApiSource>(new OsmApiSource(provider, region));
}

OsmApiSource::OsmApiSource(const SourceProvider &provider, uint32_t region)
    : Region_(provider.Coverage ? std::optional(region) : std::nullopt) {
  Decl_.Id = provider.Dataset;
  Decl_.Revision = provider.Revision;
  Decl_.Endpoint =
      provider.Coverage ? MapEndpoint(provider.Endpoint, *provider.Coverage) : provider.Endpoint;
  Decl_.Kind = DataKind::OriginalOsm;
  Decl_.How = provider.Coverage ? Scheme::WholeWorld : Scheme::GeodeticGrid;
  Decl_.Wire = WireFormat::OsmXml;
  Decl_.Order = static_cast<Rank>(provider.Priority);
  Decl_.OnAbsent = AbsencePolicy::Fail;
  Decl_.Latency = LatencyClass::Regional;
  Decl_.MaximumPayloadBytes = kMaxOsmXmlBytes;
  Decl_.PayloadSha256 = provider.PayloadSha256;
  Decl_.RetryBudget = 4;
}

Coverage OsmApiSource::Covers(const Fetch &request) const noexcept {
  return request.Kind() == DataKind::OriginalOsm && Accepts(request.Where()) ? Coverage::Inside
                                                                             : Coverage::Outside;
}

bool OsmApiSource::Accepts(const Address &at) const noexcept {
  if (Region_) { return at.Index() == Region_; }
  return ApiCellBounds(at).has_value();
}

FetchStart OsmApiSource::Begin(const Address &at, Transport &transport) const {
  if (Region_) {
    if (at.Index() != Region_) { return std::unexpected(FetchFailureReason::InvalidRequest); }
    return transport.Begin(Decl_.Endpoint);
  }
  const auto bounds = ApiCellBounds(at);
  if (!bounds) { return std::unexpected(FetchFailureReason::InvalidRequest); }
  return transport.Begin(MapEndpoint(Decl_.Endpoint, *bounds));
}

Fetched OsmApiSource::Collect(const Address &at, Ticket ticket, Transport &transport) const {
  if (!Accepts(at)) { return Fetched::Meant(Meaning::Refused, FetchFailureReason::InvalidRequest); }
  auto wire = transport.Collect(ticket);
  if (wire.Where() == Wire::State::Working) { return Fetched::Working(); }
  if (wire.Where() == Wire::State::Unreachable) {
    const auto reason = wire.FailureReason();
    const bool terminal =
        reason == FetchFailureReason::Cancelled || reason == FetchFailureReason::CapacityRefused;
    return Fetched::Meant(terminal ? Meaning::Refused : Meaning::Retry, reason);
  }
  const double retryAfterS = wire.RetryAfterS();
  auto response = wire.Take();
  if (!response) { return Fetched::Meant(Meaning::Refused, wire.FailureReason()); }
  if (response->Status == kHttpOk && !response->Body.empty()) {
    return Fetched::Delivered(std::move(response->Body));
  }
  if (response->Status == kHttpBadRequest) {
    const std::string_view message(reinterpret_cast<const char *>(response->Body.data()),
                                   response->Body.size());
    if (message.starts_with("You requested too many nodes (limit is ")) {
      return Fetched::Meant(Meaning::Refused, FetchFailureReason::CapacityRefused);
    }
  }
  if (response->Status == kHttpNotFound) { return Fetched::NotFound(); }
  if (response->Status == kHttpRequestTimeout || response->Status == kHttpTooManyRequests ||
      response->Status >= kHttpServerError) {
    return Fetched::MeantAfter(Meaning::Retry, retryAfterS);
  }
  return Fetched::Meant(Meaning::Refused);
}

}
