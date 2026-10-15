#ifndef OUTSHINE_WORLD_WEATHER_WEATHERSNAPSHOT_H
#define OUTSHINE_WORLD_WEATHER_WEATHERSNAPSHOT_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace outshine {

/// Requested geodetic location; surface height is neither camera nor model-grid height.
struct WeatherPosition {
  double LatitudeDeg = 0.0;  ///< Degrees north in [-90,90].
  double LongitudeDeg = 0.0; ///< Degrees east in [-180,180].
  std::optional<double> DeclaredSurfaceElevationAslM =
      std::nullopt; ///< Explicit downscaling request, metres ASL.
};

/// Horizontal ENU velocity measured/modelled at 10 metres above ground; no vertical component.
struct WeatherWind10m {
  double EastMs = 0.0;  ///< Eastward velocity in metres per second.
  double NorthMs = 0.0; ///< Northward velocity in metres per second.
};

/// Amounts over the explicit preceding interval, not instantaneous rates or surface wetness.
struct WeatherPrecipitation {
  int64_t BeginUtcS = 0;                               ///< Interval start, signed Unix UTC seconds.
  int64_t EndUtcS = 0;                                 ///< Interval end, signed Unix UTC seconds.
  std::optional<double> TotalWaterKgM2 = std::nullopt; ///< Total water-equivalent amount, kg/m².
  std::optional<double> RainWaterKgM2 = std::nullopt;  ///< Rain water-equivalent amount, kg/m².
  std::optional<double> ShowersWaterKgM2 =
      std::nullopt; ///< Showers water-equivalent amount, kg/m².
  std::optional<double> SnowfallDepthM =
      std::nullopt; ///< New snowfall depth, metres; not snow mass.
};

/// Owned native values at one exact UTC. Empty optionals mean unknown; zero means known zero.
/// No interpolation, procedural completion, IO or synchronization is implicit in this value.
struct WeatherValues {
  int64_t ValidUtcS = 0; ///< Exact validity time, signed Unix UTC seconds.
  std::optional<double> CloudTotal =
      std::nullopt; ///< Total cover fraction [0,1], independent of layer overlap.
  std::optional<double> CloudLow =
      std::nullopt; ///< Low cloud cover fraction [0,1]; no implied base height.
  std::optional<double> CloudMid = std::nullopt;       ///< Middle cloud cover fraction [0,1].
  std::optional<double> CloudHigh = std::nullopt;      ///< High cloud cover fraction [0,1].
  std::optional<double> WindSpeed10mMs = std::nullopt; ///< Horizontal speed at 10 m AGL, m/s.
  std::optional<double> WindFrom10mDeg =
      std::nullopt; ///< Meteorological clockwise bearing from north, [0,360).
  std::optional<WeatherWind10m> Wind10m =
      std::nullopt; ///< Horizontal ENU velocity at 10 m AGL, m/s.
  std::optional<double> AirTemperature2mK = std::nullopt; ///< Air temperature at 2 m AGL, kelvin.
  std::optional<double> RelativeHumidity2m =
      std::nullopt; ///< Relative humidity at 2 m AGL, fraction [0,1].
  std::optional<double> VisibilityM =
      std::nullopt; ///< Horizontal visibility, metres; not an aerosol multiplier.
  std::optional<double> SurfacePressurePa = std::nullopt; ///< Surface pressure, pascals.
  std::optional<double> SnowDepthM =
      std::nullopt; ///< Surface snow depth, metres; not mass or snow coverage.
  std::optional<double> CloudBaseAglM =
      std::nullopt; ///< Known cloud base, metres above local ground.
  std::optional<double> VerticalWindMs =
      std::nullopt; ///< Known upward velocity, m/s; no inference from 10 m wind.
  WeatherPrecipitation Precipitation{}; ///< Explicit preceding-interval amounts.
};

/// Origin of supplied values; procedural consumer recipes remain separate from the source.
enum class WeatherOrigin : uint8_t {
  Unknown,            ///< No origin asserted.
  ModelForecast,      ///< Model forecast, not an observation.
  ScenarioDeclaration ///< Explicit scenario weather, not a fallback for unavailable data.
};

/// Owned provenance shared by the values of one published snapshot.
struct WeatherProvenance {
  WeatherOrigin Origin = WeatherOrigin::Unknown; ///< Semantics of the supplied values.
  std::string SourceId; ///< Configured dataset identity, separate from response bytes.
  std::string
      SourceRevision;   ///< Immutable configured revision, separate from model/acquisition UTC.
  std::string Provider; ///< Provider identity; empty when unspecified.
  WeatherPosition Requested{}; ///< Original requested position, independent of model/camera.
  std::optional<double> ModelGridLatitudeDeg =
      std::nullopt; ///< Returned grid latitude, degrees north.
  std::optional<double> ModelGridLongitudeDeg =
      std::nullopt; ///< Returned grid longitude, degrees east.
  std::optional<double> DownscalingSurfaceElevationAslM =
      std::nullopt; ///< Returned downscaling height, metres ASL.
  std::optional<int64_t> SourceUtcOffsetS =
      std::nullopt;          ///< Response timezone metadata; never added to UTC.
  std::string RequestUri;    ///< Actual declared source request, not a reconstructed URL.
  std::string PayloadSha256; ///< SHA-256 of original response bytes, not normalized values.
  std::optional<int64_t> AcquiredUtcS =
      std::nullopt; ///< Recorded acquisition, whole Unix UTC seconds if known.
  std::optional<int64_t> ModelRunUtcS =
      std::nullopt; ///< Model initialization UTC, independent of acquisition.
  std::optional<std::vector<std::string>> RequestedModels =
      std::nullopt; ///< Explicit model IDs if requested.
};

/// Owned immutable-by-convention native weather publication. Copying may allocate provenance.
/// Publish after decoding on a worker; serialize mutation and keep ownership with the consumer.
struct WeatherSnapshot {
  WeatherValues Values{};     ///< Exact-time native values, with missing fields preserved.
  WeatherProvenance Source{}; ///< Original request, grid, timing and source-byte identity.
};

}
#endif
