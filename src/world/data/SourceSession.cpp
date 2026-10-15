#include "SourceSession.h"
#include "SourceConfiguration.h"
#include <utility>
#include <expected>
#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace outshine::Data {
std::expected<std::shared_ptr<SourceSession>, std::string>
SourceSession::Open(const ContentStore::Config &config,
                    std::span<const SourceProvider> providers,
                    std::string_view shippedRoot,
                    const ProviderRegistry &registry) {
  auto session = std::shared_ptr<SourceSession>(new SourceSession(config));
  std::string error;
  if (!RegisterSources(session->Sources_, providers, shippedRoot, error, registry)) {
    return std::unexpected(std::move(error));
  }
  session->Sources_.Seal();
  return session;
}
}
