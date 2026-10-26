#ifndef OUTSHINE_SOURCES_SOURCESESSION_H
#define OUTSHINE_SOURCES_SOURCESESSION_H

#include "RawSourceCache.h"
#include "SourceSet.h"
#include <Provider.h>
#include <SourceProvider.h>
#include <expected>
#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace outshine::Data {
class SourceSession {
public:
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
