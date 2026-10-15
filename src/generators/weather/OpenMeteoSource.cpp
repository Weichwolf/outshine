#include "OpenMeteoSource.h"
#include "OpenMeteo.h"

#include <utility>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>

namespace outshine::Generators::Weather {
namespace {
constexpr int kHttpOk = 200;
constexpr int kHttpNotFound = 404;

WeatherSnapshot
Snapshot(const Forecast &forecast, const HourlyWeather &hour, const Data::SourceDecl &declaration) {
  return {.Values = hour,
          .Source = {.Origin = WeatherOrigin::ModelForecast,
                     .SourceId = declaration.Id,
                     .SourceRevision = declaration.Revision,
                     .Provider = "open-meteo",
                     .Requested = forecast.Source.Requested,
                     .ModelGridLatitudeDeg = forecast.ModelGridLatitudeDeg,
                     .ModelGridLongitudeDeg = forecast.ModelGridLongitudeDeg,
                     .DownscalingSurfaceElevationAslM = forecast.DownscalingSurfaceElevationAslM,
                     .SourceUtcOffsetS = forecast.SourceUtcOffsetS,
                     .RequestUri = forecast.Source.RequestUri,
                     .PayloadSha256 = forecast.Source.PayloadSha256,
                     .AcquiredUtcS = forecast.Source.AcquiredUtcS,
                     .ModelRunUtcS = forecast.Source.ModelRunUtcS,
                     .RequestedModels = forecast.Source.RequestedModels}};
}
}

OpenMeteoSource::OpenMeteoSource(const Data::SourceProvider &declaration,
                                 const OpenMeteoRequest &request)
    : Context_{.Requested = request.Requested,
               .RequestUri = declaration.Endpoint,
               .PayloadSha256 = declaration.PayloadSha256,
               .AcquiredUtcS = request.AcquiredUtcS,
               .ModelRunUtcS = request.ModelRunUtcS,
               .RequestedModels = request.RequestedModels,
               .HttpStatus = kHttpOk} {
  Decl_.Id = declaration.Dataset;
  Decl_.Revision = declaration.Revision;
  Decl_.Endpoint = declaration.Endpoint;
  Decl_.Kind = Data::DataKind::Weather;
  Decl_.How = Data::Scheme::WholeWorld;
  Decl_.Wire = Data::WireFormat::ProviderDefined;
  Decl_.Order = static_cast<Data::Rank>(declaration.Priority);
  Decl_.OnAbsent = declaration.Missing;
  Decl_.MaximumPayloadBytes = kOpenMeteoMaximumBytes;
  Decl_.PayloadSha256 = declaration.PayloadSha256;
}

Data::Coverage OpenMeteoSource::Covers(const Data::Fetch &request) const noexcept {
  return request.Kind() == Data::DataKind::Weather && request.Where().Index() == 0
             ? Data::Coverage::Inside
             : Data::Coverage::Outside;
}

Data::FetchStart OpenMeteoSource::Begin(const Data::Address &at, Data::Transport &transport) const {
  if (at.Index() != 0) { return std::unexpected(Data::FetchFailureReason::InvalidRequest); }
  return transport.Begin(Decl_.Endpoint);
}

Data::Fetched OpenMeteoSource::Collect(const Data::Address &at,
                                       Data::Ticket ticket,
                                       Data::Transport &transport) const {
  if (at.Index() != 0) {
    return Data::Fetched::Meant(Data::Meaning::Refused, Data::FetchFailureReason::InvalidRequest);
  }
  auto wire = transport.Collect(ticket);
  if (wire.Where() == Data::Wire::State::Working) { return Data::Fetched::Working(); }
  const auto reason = wire.FailureReason();
  auto response = wire.Take();
  if (!response) { return Data::Fetched::Meant(Data::Meaning::Refused, reason); }
  if (response->Status == kHttpOk && !response->Body.empty() &&
      response->Body.size() <= kOpenMeteoMaximumBytes && !response->Range) {
    return Data::Fetched::Delivered(std::move(response->Body));
  }
  if (response->Status == kHttpNotFound) { return Data::Fetched::NotFound(); }
  return Data::Fetched::Meant(
      Data::Meaning::Refused, Data::FetchFailureReason::ProviderRefused, response->Status);
}

std::expected<std::optional<WeatherSnapshot>, Data::DecodeFailure>
OpenMeteoSource::DecodeWeather(std::span<const uint8_t> bytes, int64_t utcS) const {
  const auto forecast = DecodeOpenMeteo(bytes, Context_);
  if (!forecast) { return std::unexpected(Data::DecodeFailure::CorruptPayload); }
  const auto *hour = ExactHour(*forecast, utcS);
  if (hour == nullptr) { return std::nullopt; }
  return Snapshot(*forecast, *hour, Decl_);
}

}
