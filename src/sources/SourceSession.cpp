#include "SourceSession.h"
#include "SourceConfiguration.h"
#include <utility>
#include <expected>
#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace outshine::Data {
SourceSession::Configuration::Configuration(Configuration &&other) noexcept
    : Sources_(std::move(other.Sources_)), Available_(std::exchange(other.Available_, false)) {}

SourceSession::Configuration &
SourceSession::Configuration::operator=(Configuration &&other) noexcept {
  if (this != &other) {
    Sources_ = std::move(other.Sources_);
    Available_ = std::exchange(other.Available_, false);
  }
  return *this;
}

std::expected<SourceSession::Configuration, std::string>
SourceSession::Prepare(std::span<const SourceProvider> providers,
                       std::string_view builtinDataDirectory,
                       const ProviderRegistry &registry) {
  auto sources = PrepareSources(providers, builtinDataDirectory, registry);
  if (!sources) { return std::unexpected(std::move(sources.error())); }
  return Configuration(std::move(*sources));
}

std::expected<std::shared_ptr<SourceSession>, std::string>
SourceSession::Open(const RawSourceCache::Config &config, Configuration sources) {
  if (!sources.Available_) {
    return std::unexpected("source configuration has already been consumed");
  }
  auto session = std::shared_ptr<SourceSession>(new SourceSession(config));
  std::string error;
  if (!RegisterSources(session->Sources_, std::move(sources.Sources_), error)) {
    return std::unexpected(std::move(error));
  }
  session->Sources_.Seal();
  return session;
}

std::expected<std::shared_ptr<SourceSession>, std::string>
SourceSession::Open(const RawSourceCache::Config &config,
                    std::span<const SourceProvider> providers,
                    std::string_view builtinDataDirectory,
                    const ProviderRegistry &registry) {
  auto sources = Prepare(providers, builtinDataDirectory, registry);
  if (!sources) { return std::unexpected(std::move(sources.error())); }
  return Open(config, std::move(*sources));
}
}
