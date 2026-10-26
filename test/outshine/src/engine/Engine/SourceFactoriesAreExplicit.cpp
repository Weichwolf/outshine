#include "Engine.h"
#include "Check.h"
#include <expected>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace {
class Probe final : public outshine::Data::Provider {
public:
  mutable unsigned Validations = 0;
  mutable unsigned Constructions = 0;

  [[nodiscard]] std::string_view kind() const override { return "stars"; }

  [[nodiscard]] std::expected<void, std::string> validate(
      [[maybe_unused]] const outshine::Data::SourceProvider &declaration,
      [[maybe_unused]] std::span<const outshine::Data::SourceProvider> declared) const override {
    ++Validations;
    return {};
  }

  [[nodiscard]] std::expected<std::unique_ptr<outshine::Data::Source>, std::string>
  make([[maybe_unused]] const outshine::Data::SourceProvider &declaration,
       [[maybe_unused]] std::string_view shippedRoot) const override {
    ++Constructions;
    return std::unexpected("fixture forbids source construction");
  }
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const Probe provider;
  Scenario::Document original;
  original.Named.Name = "kept";
  Scenario::Document requested = original;
  Data::SourceProvider declaration;
  declaration.Kind = "stars";
  requested.Providers.push_back(std::move(declaration));

  Engine empty;
  CHECK(empty.declare(original), "a host without providers accepts a source-free declaration");
  const auto before = empty.writeScenario();
  const auto missing = empty.declare(requested);
  CHECK(!missing && !missing.error().empty(), "unregistered source kind is rejected before IO");
  CHECK(empty.writeScenario() == before, "missing factory preserves the published declaration");
  CHECK(empty.registerProvider(provider), "declaration never silently installs builtin factories");
  CHECK(empty.declare(requested), "explicit individual registration satisfies the same request");

  Data::ProviderRegistry registry;
  CHECK(registry.registerProvider(provider), "caller owns factory selection");
  Engine configured(std::move(registry));
  CHECK(configured.declare(requested), "host consumes the caller's complete factory registry");
  CHECK(!configured.registerProvider(provider), "registry ownership preserves duplicate detection");
  CHECK(provider.Validations == 2 && provider.Constructions == 0,
        "validation reaches the selected factory without constructing or acquiring sources");
  return Report();
}
