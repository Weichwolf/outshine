#include "OsmApiSource.h"

#include "OsmXmlReader.h"
#include "SourceProviderValidation.h"
#include <world/SourceProvider.h>

#include <format>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <string>
#include <utility>

namespace outshine::Data {
namespace {
constexpr int kHttpOk = 200;
constexpr int kHttpNotFound = 404;
constexpr int kHttpRequestTimeout = 408;
constexpr int kHttpTooManyRequests = 429;
constexpr int kHttpServerError = 500;
}

std::expected<std::unique_ptr<OsmApiSource>, std::string>
OsmApiSource::Create(const SourceProvider &provider, uint32_t region) {
  if (provider.Kind != "osm" || provider.Endpoint.empty() || !provider.Coverage) {
    return std::unexpected("an original OSM API source requires an osm endpoint declaration");
  }
  if (auto valid = ValidateSourceProviders(std::span(&provider, 1)); !valid) {
    return std::unexpected(std::move(valid.error()));
  }
  return std::unique_ptr<OsmApiSource>(new OsmApiSource(provider, *provider.Coverage, region));
}

OsmApiSource::OsmApiSource(const SourceProvider &provider, SourceCoverage bounds, uint32_t region)
    : Region_(region) {
  Decl_.Id = provider.Dataset;
  Decl_.Revision = provider.Revision;
  Decl_.Endpoint = std::format("{}/map?bbox={},{},{},{}",
                               provider.Endpoint,
                               bounds.WestDeg,
                               bounds.SouthDeg,
                               bounds.EastDeg,
                               bounds.NorthDeg);
  Decl_.Kind = DataKind::OriginalOsm;
  Decl_.How = Scheme::WholeWorld;
  Decl_.Wire = WireFormat::OsmXml;
  Decl_.Order = static_cast<Rank>(provider.Priority);
  Decl_.OnAbsent = AbsencePolicy::Fail;
  Decl_.Latency = LatencyClass::Regional;
  Decl_.MaximumPayloadBytes = kMaxOsmXmlBytes;
  Decl_.PayloadSha256 = provider.PayloadSha256;
  Decl_.RetryBudget = 4;
}

Coverage OsmApiSource::Covers(const Fetch &request) const noexcept {
  return request.Kind() == DataKind::OriginalOsm && request.Where().Index() == Region_
             ? Coverage::Inside
             : Coverage::Outside;
}

FetchStart OsmApiSource::Begin(const Address &at, Transport &transport) const {
  if (at.Index() != Region_) { return std::unexpected(FetchFailureReason::InvalidRequest); }
  return transport.Begin(Decl_.Endpoint);
}

Fetched OsmApiSource::Collect(const Address &at, Ticket ticket, Transport &transport) const {
  if (at.Index() != Region_) {
    return Fetched::Meant(Meaning::Refused, FetchFailureReason::InvalidRequest);
  }
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
  if (response->Status == kHttpNotFound) { return Fetched::NotFound(); }
  if (response->Status == kHttpRequestTimeout || response->Status == kHttpTooManyRequests ||
      response->Status >= kHttpServerError) {
    return Fetched::MeantAfter(Meaning::Retry, retryAfterS);
  }
  return Fetched::Meant(Meaning::Refused);
}

}
