#ifndef OUTSHINE_GENERATION_WEATHER_OPENMETEOPROVIDER_H
#define OUTSHINE_GENERATION_WEATHER_OPENMETEOPROVIDER_H

#include <Provider.h>
#include <WeatherSnapshot.h>

namespace outshine::Generators::Weather {

struct OpenMeteoRequest {
  WeatherPosition Requested{};
  std::optional<int64_t> AcquiredUtcS = std::nullopt;
  std::optional<int64_t> ModelRunUtcS = std::nullopt;
  std::optional<std::vector<std::string>> RequestedModels = std::nullopt;
};

class OpenMeteoProvider final : public Data::Provider {
public:
  explicit OpenMeteoProvider(OpenMeteoRequest request);

  [[nodiscard]] std::string_view kind() const override;

  [[nodiscard]] std::expected<std::unique_ptr<Data::Source>, std::string>
  make(const Data::SourceProvider &declaration, std::string_view shippedRoot) const override;

private:
  OpenMeteoRequest Request_;
};

}
#endif
