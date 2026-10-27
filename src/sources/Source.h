#ifndef OUTSHINE_SOURCES_SOURCE_H
#define OUTSHINE_SOURCES_SOURCE_H

#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <HeightRaster.h>
#include <WeatherSnapshot.h>

#include "SourceResponse.h"
#include "SourceRequest.h"
#include "SourceDecl.h"
#include "SourceTransport.h"

namespace outshine::Data {

enum class Coverage : uint8_t { Inside, Outside };

enum class DecodeFailure : uint8_t { Unsupported, CorruptPayload };

class Source {
public:
  virtual ~Source() = default;

  [[nodiscard]] virtual const SourceDecl &Declaration() const noexcept = 0;

  [[nodiscard]] virtual Coverage Covers(const SourceRequest &request) const noexcept = 0;

  [[nodiscard]] virtual Address Serves(const SourceRequest &request) const noexcept = 0;

  [[nodiscard]] virtual FetchStart Begin(const Address &at, SourceTransport &transport) const = 0;

  [[nodiscard]] virtual FetchStart Begin(const SourceRequest &request,
                                         SourceTransport &transport) const {
    if (request.Range()) { return std::unexpected(SourceFailureReason::ProviderRefused); }
    return Begin(request.Where(), transport);
  }

  [[nodiscard]] virtual SourceResponse
  Collect(const Address &at, Ticket ticket, SourceTransport &transport) const = 0;

  [[nodiscard]] virtual std::expected<HeightRaster, DecodeFailure>
  DecodeElevation([[maybe_unused]] std::span<const uint8_t> bytes) const {
    return std::unexpected(DecodeFailure::Unsupported);
  }

  [[nodiscard]] virtual std::expected<std::optional<WeatherSnapshot>, DecodeFailure>
  DecodeWeather([[maybe_unused]] std::span<const uint8_t> bytes,
                [[maybe_unused]] int64_t utcS) const {
    return std::unexpected(DecodeFailure::Unsupported);
  }

  virtual void Cancel(Ticket ticket, SourceTransport &transport) const { transport.Cancel(ticket); }
};

}
#endif
