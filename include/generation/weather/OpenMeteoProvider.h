#ifndef OUTSHINE_GENERATION_WEATHER_OPENMETEOPROVIDER_H
#define OUTSHINE_GENERATION_WEATHER_OPENMETEOPROVIDER_H

#include <world/Provider.h>
#include <world/weather/WeatherSnapshot.h>

namespace outshine::Generators::Weather {

/// Typed request context supplied by the caller that owns the actual request/receipt.
/// Coordinates are not parsed back from a URL. Unknown acquisition/model times stay unknown.
struct OpenMeteoRequest {
  WeatherPosition Requested{}; ///< Original request, including an explicit downscaling height.
  std::optional<int64_t> AcquiredUtcS =
      std::nullopt; ///< Recorded whole-second acquisition, not current time.
  std::optional<int64_t> ModelRunUtcS =
      std::nullopt; ///< Model initialization only if independently known.
  std::optional<std::vector<std::string>> RequestedModels =
      std::nullopt; ///< Explicit requested model IDs.
};

/// Configured Open-Meteo extension factory, registered through the ordinary ProviderRegistry.
/// SourceProvider supplies kind=open-meteo, dataset/revision, exact HTTPS endpoint and mandatory
/// payload SHA-256 pin. The immutable capture has one Whole(0) address; no automatic refresh.
/// Each returned source owns all request context. Factory make and construction perform no IO.
class OpenMeteoProvider final : public Data::Provider {
public:
  /// Copy/move the trusted request context into this factory; may allocate owned model IDs.
  /// @param request Original typed request and recorded receipt metadata.
  explicit OpenMeteoProvider(OpenMeteoRequest request);

  /// Identify this provider for explicit scenario/source registration.
  /// @return Process-lifetime name; no IO or allocation.
  [[nodiscard]] std::string_view kind() const override;

  /// Validate one immutable pinned forecast capture and create its source without IO.
  /// @param declaration Owned-by-caller configuration, copied into the returned source.
  /// @param shippedRoot Unused: bytes use the existing transport and source cache.
  /// @return Owned source or diagnostic; bounded setup allocation only.
  [[nodiscard]] std::expected<std::unique_ptr<Data::Source>, std::string>
  make(const Data::SourceProvider &declaration, std::string_view shippedRoot) const override;

private:
  OpenMeteoRequest Request_;
};

}
#endif
