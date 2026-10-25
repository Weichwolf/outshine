#ifndef OUTSHINE_GENERATORS_OSM_PROVIDERS_OSMPROVIDERVALIDATION_H
#define OUTSHINE_GENERATORS_OSM_PROVIDERS_OSMPROVIDERVALIDATION_H

#include <SourceProvider.h>
#include <expected>
#include <span>
#include <string>
#include <string_view>

namespace outshine::Generators::Osm {
inline constexpr std::string_view kOfficialOsmApi = "https://api.openstreetmap.org/api/0.6";
inline constexpr double kOsmApiMaximumAreaDeg2 = 0.25;

[[nodiscard]] std::expected<void, std::string>
ValidateProvider(const Data::SourceProvider &provider,
                 std::span<const Data::SourceProvider> declared);
[[nodiscard]] std::expected<void, std::string>
ValidateProviders(std::span<const Data::SourceProvider> providers);
}
#endif
