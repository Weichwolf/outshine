#ifndef OUTSHINE_SOURCES_SOURCECONFIGURATION_H
#define OUTSHINE_SOURCES_SOURCECONFIGURATION_H

#include <span>
#include <string>
#include <string_view>
#include <expected>
#include <memory>

#include <SourceProvider.h>
#include <Provider.h>

#include "SourceSet.h"

namespace outshine::Data {

[[nodiscard]] bool RegisterSources(SourceSet &set,
                                   std::span<const SourceProvider> providers,
                                   std::string_view builtinDataDirectory,
                                   std::string &error,
                                   const ProviderRegistry &registry);

[[nodiscard]] std::expected<std::unique_ptr<Source>, std::string>
ConfigureSource(const SourceProvider &provider,
                std::string_view builtinDataDirectory,
                const ProviderRegistry &registry);

}
#endif
