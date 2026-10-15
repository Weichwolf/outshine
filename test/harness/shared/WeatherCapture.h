#ifndef OUTSHINE_TEST_WEATHERCAPTURE_H
#define OUTSHINE_TEST_WEATHERCAPTURE_H

#include <generation/weather/OpenMeteoProvider.h>
#include "format/Json.h"
#include "io/ReadTextFile.h"

#include <optional>
#include <expected>
#include <vector>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>

namespace outshine::Test {

inline constexpr int64_t kCapturedWeatherUtcS = 1792022400;
inline constexpr int64_t kCapturedWeatherAcquiredUtcS = 1792066383;
inline constexpr int64_t kWeatherSecondsPerHour = 3600;
inline constexpr int64_t kWeatherSecondsPerDay = 24 * kWeatherSecondsPerHour;
inline constexpr double kCapturedWeatherLatitudeDeg = 47.256121;
inline constexpr double kCapturedWeatherLongitudeDeg = 10.115857;
inline constexpr double kCapturedWeatherElevationAslM = 1772.0;
inline constexpr double kCapturedWeatherVisibilityM = 42140.0;
inline constexpr double kDeclaredLowCover = 0.65;
inline constexpr double kDeclaredCloudBaseAslM = 2572.0;
inline constexpr double kDeclaredCloudThicknessM = 600.0;
inline constexpr double kDeclaredCloudExtinctionPerM = 0.012;
inline constexpr double kDeclaredCloudAdvectionMs = 8.0;
inline constexpr int kWeatherHttpOk = 200;

struct WeatherCapture {
  std::string Bytes;
  Data::SourceProvider Declaration;
  Generators::Weather::OpenMeteoRequest Request;
};

inline std::optional<WeatherCapture> RecordedWeatherCapture(bool withPayload = true) {
  const auto receipt =
      ReadTextFile("test/experiments/weather/data/open-meteo/koerbersee-20261015.receipt.json",
                   size_t{64} * 1024);
  if (!receipt) { return std::nullopt; }
  Json metadata;
  if (!metadata.Parse(receipt->data(), receipt->size())) { return std::nullopt; }
  const auto root = metadata.Root();
  Data::SourceProvider declaration;
  declaration.Kind = "open-meteo";
  declaration.Revision = "koerbersee-20261015";
  declaration.Missing = Data::MissingDataPolicy::Fail;
  declaration.Dataset = "koerbersee-forecast";
  declaration.Endpoint = std::string(root["url"].Str());
  declaration.PayloadSha256 = std::string(root["source_sha256"].Str());
  std::string bytes;
  if (withPayload) {
    auto loaded = ReadTextFile("test/experiments/weather/data/open-meteo/koerbersee-20261015.json",
                               size_t{1024} * 1024);
    if (!loaded) { return std::nullopt; }
    bytes = std::move(*loaded);
  }
  return WeatherCapture{
      .Bytes = std::move(bytes),
      .Declaration = std::move(declaration),
      .Request = {.Requested = {.LatitudeDeg = kCapturedWeatherLatitudeDeg,
                                .LongitudeDeg = kCapturedWeatherLongitudeDeg,
                                .DeclaredSurfaceElevationAslM = kCapturedWeatherElevationAslM},
                  .AcquiredUtcS = kCapturedWeatherAcquiredUtcS}};
}

class CapturedWeatherTransport final : public Data::Transport {
public:
  explicit CapturedWeatherTransport(const WeatherCapture &capture) : Capture_(capture) {}

  Data::FetchStart Begin(const std::string &url) override {
    if (url != Capture_.Declaration.Endpoint) {
      return std::unexpected(Data::FetchFailureReason::InvalidRequest);
    }
    Active_ = static_cast<Data::Ticket>(++Begins_);
    return Active_;
  }

  Data::Wire Collect(Data::Ticket ticket) override {
    if (ticket != Active_ || Active_ == Data::Ticket::None) {
      return Data::Wire::Unreachable(Data::FetchFailureReason::InvalidRequest);
    }
    Active_ = Data::Ticket::None;
    return Data::Wire::Answered(kWeatherHttpOk,
                                std::vector<uint8_t>(Capture_.Bytes.begin(), Capture_.Bytes.end()));
  }

  void Cancel(Data::Ticket ticket) override {
    if (ticket == Active_ && Active_ != Data::Ticket::None) {
      ++Cancels_;
      Active_ = Data::Ticket::None;
    }
  }

  [[nodiscard]] int Begins() const noexcept { return Begins_; }

  [[nodiscard]] int Cancels() const noexcept { return Cancels_; }

private:
  const WeatherCapture &Capture_;
  int Begins_ = 0;
  int Cancels_ = 0;
  Data::Ticket Active_ = Data::Ticket::None;
};

inline std::span<const uint8_t> WeatherBytes(const WeatherCapture &capture) {
  return {reinterpret_cast<const uint8_t *>(capture.Bytes.data()), capture.Bytes.size()};
}

}
#endif
