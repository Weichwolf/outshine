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

namespace outshine::Generators::Osm {
namespace {
constexpr int kHttpOk = 200;
constexpr int kHttpBadRequest = 400;
constexpr int kHttpNotFound = 404;
constexpr int kHttpRequestTimeout = 408;
constexpr int kHttpTooManyRequests = 429;
constexpr int kHttpServerError = 500;

std::string MapEndpoint(std::string_view endpoint, const Data::SourceCoverage &bounds) {
  return std::format("{}/map?bbox={},{},{},{}",
                     endpoint,
                     bounds.WestDeg,
                     bounds.SouthDeg,
                     bounds.EastDeg,
                     bounds.NorthDeg);
}

std::optional<Data::SourceCoverage> ApiCellBounds(const Data::Address &at) noexcept {
  const auto cell = at.GeoCell();
  if (!cell) { return std::nullopt; }
  const auto bounds = cell->Bounds();
  if (!bounds || (bounds->EastDeg - bounds->WestDeg) * (bounds->NorthDeg - bounds->SouthDeg) >
                     Data::kOsmApiMaximumAreaDeg2) {
    return std::nullopt;
  }
  return bounds;
}
}

std::expected<std::unique_ptr<ApiSource>, std::string>
ApiSource::Create(const Data::SourceProvider &provider, uint32_t region) {
  if (provider.Kind != "osm" || provider.Endpoint.empty()) {
    return std::unexpected("an original OSM API source requires an osm endpoint declaration");
  }
  if (auto valid = Data::ValidateSourceProviders(std::span(&provider, 1)); !valid) {
    return std::unexpected(std::move(valid.error()));
  }
  return std::unique_ptr<ApiSource>(new ApiSource(provider, region));
}

ApiSource::ApiSource(const Data::SourceProvider &provider, uint32_t region)
    : Region_(provider.Coverage ? std::optional(region) : std::nullopt) {
  Decl_.Id = provider.Dataset;
  Decl_.Revision = provider.Revision;
  Decl_.Endpoint =
      provider.Coverage ? MapEndpoint(provider.Endpoint, *provider.Coverage) : provider.Endpoint;
  Decl_.Kind = Data::DataKind::OriginalOsm;
  Decl_.How = provider.Coverage ? Data::Scheme::WholeWorld : Data::Scheme::GeodeticGrid;
  Decl_.Wire = Data::WireFormat::OsmXml;
  Decl_.Order = static_cast<Data::Rank>(provider.Priority);
  Decl_.OnAbsent = Data::AbsencePolicy::Fail;
  Decl_.Latency = Data::LatencyClass::Regional;
  Decl_.MaximumPayloadBytes = Data::kMaxOsmXmlBytes;
  Decl_.PayloadSha256 = provider.PayloadSha256;
  Decl_.RetryBudget = 4;
}

Data::Coverage ApiSource::Covers(const Data::Fetch &request) const noexcept {
  return request.Kind() == Data::DataKind::OriginalOsm && Accepts(request.Where())
             ? Data::Coverage::Inside
             : Data::Coverage::Outside;
}

bool ApiSource::Accepts(const Data::Address &at) const noexcept {
  if (Region_) { return at.Index() == Region_; }
  return ApiCellBounds(at).has_value();
}

Data::FetchStart ApiSource::Begin(const Data::Address &at, Data::Transport &transport) const {
  if (Region_) {
    if (at.Index() != Region_) { return std::unexpected(Data::FetchFailureReason::InvalidRequest); }
    return transport.Begin(Decl_.Endpoint);
  }
  const auto bounds = ApiCellBounds(at);
  if (!bounds) { return std::unexpected(Data::FetchFailureReason::InvalidRequest); }
  return transport.Begin(MapEndpoint(Decl_.Endpoint, *bounds));
}

Data::Fetched
ApiSource::Collect(const Data::Address &at, Data::Ticket ticket, Data::Transport &transport) const {
  if (!Accepts(at)) {
    return Data::Fetched::Meant(Data::Meaning::Refused, Data::FetchFailureReason::InvalidRequest);
  }
  auto wire = transport.Collect(ticket);
  if (wire.Where() == Data::Wire::State::Working) { return Data::Fetched::Working(); }
  if (wire.Where() == Data::Wire::State::Unreachable) {
    const auto reason = wire.FailureReason();
    const bool terminal = reason == Data::FetchFailureReason::Cancelled ||
                          reason == Data::FetchFailureReason::CapacityRefused;
    return Data::Fetched::Meant(terminal ? Data::Meaning::Refused : Data::Meaning::Retry, reason);
  }
  const double retryAfterS = wire.RetryAfterS();
  auto response = wire.Take();
  if (!response) { return Data::Fetched::Meant(Data::Meaning::Refused, wire.FailureReason()); }
  if (response->Status == kHttpOk && !response->Body.empty()) {
    return Data::Fetched::Delivered(std::move(response->Body));
  }
  if (response->Status == kHttpBadRequest) {
    const std::string_view message(reinterpret_cast<const char *>(response->Body.data()),
                                   response->Body.size());
    if (message.starts_with("You requested too many nodes (limit is ")) {
      return Data::Fetched::Meant(Data::Meaning::Refused,
                                  Data::FetchFailureReason::CapacityRefused);
    }
  }
  if (response->Status == kHttpNotFound) { return Data::Fetched::NotFound(); }
  if (response->Status == kHttpRequestTimeout || response->Status == kHttpTooManyRequests ||
      response->Status >= kHttpServerError) {
    return Data::Fetched::MeantAfter(Data::Meaning::Retry, retryAfterS);
  }
  return Data::Fetched::Meant(Data::Meaning::Refused);
}

}
