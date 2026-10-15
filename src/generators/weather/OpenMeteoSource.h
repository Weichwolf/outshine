#ifndef OUTSHINE_GENERATORS_WEATHER_OPENMETEOSOURCE_H
#define OUTSHINE_GENERATORS_WEATHER_OPENMETEOSOURCE_H

#include "Forecast.h"
#include <generation/weather/OpenMeteoProvider.h>

namespace outshine::Generators::Weather {

class OpenMeteoSource final : public Data::Source {
public:
  OpenMeteoSource(const Data::SourceProvider &declaration, const OpenMeteoRequest &request);

  [[nodiscard]] const Data::SourceDecl &Declaration() const noexcept override { return Decl_; }

  [[nodiscard]] Data::Coverage Covers(const Data::Fetch &request) const noexcept override;

  [[nodiscard]] Data::Address Serves(const Data::Fetch &request) const noexcept override {
    return request.Where();
  }

  [[nodiscard]] Data::FetchStart Begin(const Data::Address &at,
                                       Data::Transport &transport) const override;
  [[nodiscard]] Data::Fetched
  Collect(const Data::Address &at, Data::Ticket ticket, Data::Transport &transport) const override;
  [[nodiscard]] std::expected<std::optional<WeatherSnapshot>, Data::DecodeFailure>
  DecodeWeather(std::span<const uint8_t> bytes, int64_t utcS) const override;

private:
  Data::SourceDecl Decl_;
  ForecastSource Context_;
};

}
#endif
