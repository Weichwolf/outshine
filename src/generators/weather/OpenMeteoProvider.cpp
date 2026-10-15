#include <generation/weather/OpenMeteoProvider.h>
#include "OpenMeteoSource.h"

#include <algorithm>
#include <cmath>
#include <expected>
#include <memory>
#include <string_view>
#include <ranges>
#include <utility>

namespace outshine::Generators::Weather {
namespace {
constexpr double kLatitudeLimitDeg = 90.0;
constexpr double kLongitudeLimitDeg = 180.0;

bool ValidRequest(const OpenMeteoRequest &request) noexcept {
  const auto &at = request.Requested;
  return std::isfinite(at.LatitudeDeg) && at.LatitudeDeg >= -kLatitudeLimitDeg &&
         at.LatitudeDeg <= kLatitudeLimitDeg && std::isfinite(at.LongitudeDeg) &&
         at.LongitudeDeg >= -kLongitudeLimitDeg && at.LongitudeDeg <= kLongitudeLimitDeg &&
         (!at.DeclaredSurfaceElevationAslM || std::isfinite(*at.DeclaredSurfaceElevationAslM));
}

bool ValidCapture(const Data::SourceProvider &declaration) {
  return declaration.Kind == "open-meteo" && !declaration.Dataset.empty() &&
         !declaration.Revision.empty() && declaration.Location.empty() && !declaration.Coverage &&
         declaration.Schema.empty() &&
         declaration.Endpoint.starts_with("https://api.open-meteo.com/v1/forecast?") &&
         declaration.Endpoint.find_first_of(" \t\r\n{}") == std::string::npos &&
         declaration.PayloadSha256.size() == 64 &&
         std::ranges::all_of(declaration.PayloadSha256, [](char digit) {
           return (digit >= '0' && digit <= '9') || (digit >= 'a' && digit <= 'f');
         });
}
}

OpenMeteoProvider::OpenMeteoProvider(OpenMeteoRequest request) : Request_(std::move(request)) {}

std::string_view OpenMeteoProvider::kind() const {
  return "open-meteo";
}

std::expected<std::unique_ptr<Data::Source>, std::string>
OpenMeteoProvider::make(const Data::SourceProvider &declaration,
                        [[maybe_unused]] std::string_view shippedRoot) const {
  if (!ValidRequest(Request_) || !ValidCapture(declaration)) {
    return std::unexpected(
        "open-meteo requires a valid typed request and immutable pinned capture");
  }
  return std::make_unique<OpenMeteoSource>(declaration, Request_);
}

}
