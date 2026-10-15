#ifndef OUTSHINE_WORLD_DATA_SOURCESESSION_H
#define OUTSHINE_WORLD_DATA_SOURCESESSION_H

#include "ContentStore.h"
#include "SourceSet.h"
#include <world/Provider.h>
#include <world/SourceProvider.h>
#include <expected>
#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace outshine::Data {
class SourceSession {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<SourceSession>, std::string>
  Open(const ContentStore::Config &config,
       std::span<const SourceProvider> providers,
       std::string_view shippedRoot,
       const ProviderRegistry &registry);

  struct Ledger {
    SourceSet::Ledger Sources;
    ContentStore::Ledger Store;
  };

  [[nodiscard]] Ledger Counters() const {
    return {.Sources = Sources_.Counters(), .Store = Store_.Counters()};
  }

  [[nodiscard]] SourceSet &Sources() noexcept { return Sources_; }

  [[nodiscard]] const SourceSet &Sources() const noexcept { return Sources_; }

  [[nodiscard]] ContentStore &Store() noexcept { return Store_; }

  [[nodiscard]] const ContentStore &Store() const noexcept { return Store_; }

private:
  explicit SourceSession(const ContentStore::Config &config) : Store_(config), Sources_(Store_) {}

  ContentStore Store_;
  SourceSet Sources_;
};
}
#endif
