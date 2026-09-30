#ifndef OUTSHINE_WORLD_DATA_SOURCEPROVIDERVALIDATION_H
#define OUTSHINE_WORLD_DATA_SOURCEPROVIDERVALIDATION_H

#include <expected>
#include <span>
#include <string>
#include <string_view>

#include <world/SourceProvider.h>

namespace outshine::Data {

inline constexpr std::string_view kOfficialOsmApi = "https://api.openstreetmap.org/api/0.6";
inline constexpr double kOsmApiMaximumAreaDeg2 = 0.25;

[[nodiscard]] std::expected<void, std::string>
ValidateSourceProviders(std::span<const SourceProvider> providers);

}

#endif
