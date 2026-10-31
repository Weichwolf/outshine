#ifndef OUTSHINE_SOURCES_SOURCESESSION_H
#define OUTSHINE_SOURCES_SOURCESESSION_H

#include "RawSourceCache.h"
#include "SourceSet.h"
#include "SourceRequest.h"
#include <Provider.h>
#include <SourceProvider.h>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <utility>

namespace outshine::Data {
class SourceSession {
public:
  class Configuration {
  public:
    Configuration(Configuration &&other) noexcept;
    Configuration &operator=(Configuration &&other) noexcept;
    ~Configuration() = default;
    Configuration(const Configuration &) = delete;
    Configuration &operator=(const Configuration &) = delete;

  private:
    friend class SourceSession;

    explicit Configuration(std::vector<std::unique_ptr<Source>> sources)
        : Sources_(std::move(sources)) {}

    std::vector<std::unique_ptr<Source>> Sources_;
    bool Available_ = true;
  };

  // Factories and metadata validation perform no IO; Open consumes these exact sources.
  [[nodiscard]] static std::expected<Configuration, std::string>
  Prepare(std::span<const SourceProvider> providers,
          std::string_view builtinDataDirectory,
          const ProviderRegistry &registry);

  [[nodiscard]] static std::expected<std::shared_ptr<SourceSession>, std::string>
  Open(const RawSourceCache::Config &config, Configuration sources);

  class Read {
  public:
    Read(Read &&other) noexcept;
    Read &operator=(Read &&other);
    ~Read();
    Read(const Read &) = delete;
    Read &operator=(const Read &) = delete;
    [[nodiscard]] SourceDelivery Poll();
    void Cancel();

  private:
    friend class SourceSession;
    Read(std::shared_ptr<SourceSession> session,
         const SourceRequest &request,
         SourceTransport &transport);
    std::shared_ptr<SourceSession> Session_;
    SourceTransport *Transport_ = nullptr;
    std::optional<SourceSet::Query> Query_;
  };

  // The transport outlives the read; serialize Poll, Cancel, moves and destruction.
  // Creating a read only selects declared sources and starts no source IO.
  [[nodiscard]] static std::expected<Read, std::string>
  Request(std::shared_ptr<SourceSession> session,
          const SourceRequest &request,
          SourceTransport &transport);

  [[nodiscard]] static std::expected<std::shared_ptr<SourceSession>, std::string>
  Open(const RawSourceCache::Config &config,
       std::span<const SourceProvider> providers,
       std::string_view builtinDataDirectory,
       const ProviderRegistry &registry);

  struct Ledger {
    SourceSet::Ledger Sources;
    RawSourceCache::Ledger Store;
  };

  [[nodiscard]] Ledger Counters() const {
    return {.Sources = Sources_.Counters(), .Store = Store_.Counters()};
  }

  [[nodiscard]] SourceSet &Sources() noexcept { return Sources_; }

  [[nodiscard]] const SourceSet &Sources() const noexcept { return Sources_; }

  [[nodiscard]] RawSourceCache &Store() noexcept { return Store_; }

  [[nodiscard]] const RawSourceCache &Store() const noexcept { return Store_; }

private:
  explicit SourceSession(const RawSourceCache::Config &config) : Store_(config), Sources_(Store_) {}

  RawSourceCache Store_;
  SourceSet Sources_;
};
}
#endif
