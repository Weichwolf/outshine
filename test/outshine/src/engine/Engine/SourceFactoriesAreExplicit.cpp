#include "Engine.h"
#include "Check.h"
#include "Address.h"
#include "DataKind.h"
#include "Source.h"
#include "SourceDecl.h"
#include "SourceFailureReason.h"
#include "SourceRequest.h"
#include "SourceResponse.h"
#include "SourceTransport.h"
#include <expected>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace {
class MetadataSource final : public outshine::Data::Source {
public:
  explicit MetadataSource(unsigned &requests) : Requests_(requests) {
    Decl_.Id = "fixture-stars";
    Decl_.Revision = "v1";
    Decl_.Kind = outshine::Data::DataKind::StarCatalogue;
    Decl_.How = outshine::Data::Scheme::WholeWorld;
    Decl_.Wire = outshine::Data::WireFormat::StarBandBinary;
  }

  [[nodiscard]] const outshine::Data::SourceDecl &Declaration() const noexcept override {
    return Decl_;
  }

  [[nodiscard]] outshine::Data::Coverage
  Covers([[maybe_unused]] const outshine::Data::SourceRequest &request) const noexcept override {
    return outshine::Data::Coverage::Inside;
  }

  [[nodiscard]] outshine::Data::Address
  Serves(const outshine::Data::SourceRequest &request) const noexcept override {
    return request.Where();
  }

  [[nodiscard]] outshine::Data::FetchStart
  Begin([[maybe_unused]] const outshine::Data::Address &address,
        [[maybe_unused]] outshine::Data::SourceTransport &transport) const override {
    ++Requests_;
    return std::unexpected(outshine::Data::SourceFailureReason::ProviderRefused);
  }

  [[nodiscard]] outshine::Data::SourceResponse
  Collect([[maybe_unused]] const outshine::Data::Address &address,
          [[maybe_unused]] outshine::Data::Ticket ticket,
          [[maybe_unused]] outshine::Data::SourceTransport &transport) const override {
    ++Requests_;
    return outshine::Data::SourceResponse::Meant(outshine::Data::Meaning::Refused);
  }

private:
  unsigned &Requests_;
  outshine::Data::SourceDecl Decl_;
};

class ExplicitProvider final : public outshine::Data::Provider {
public:
  mutable unsigned Validations = 0;
  mutable unsigned Constructions = 0;
  mutable unsigned Requests = 0;
  mutable unsigned UnvalidatedConstructions = 0;
  mutable unsigned PreviousValidation = 0;

  [[nodiscard]] std::string_view kind() const override { return "stars"; }

  [[nodiscard]] std::expected<void, std::string> validate(
      [[maybe_unused]] const outshine::Data::SourceProvider &declaration,
      [[maybe_unused]] std::span<const outshine::Data::SourceProvider> declared) const override {
    ++Validations;
    return {};
  }

  [[nodiscard]] std::expected<std::unique_ptr<outshine::Data::Source>, std::string>
  make([[maybe_unused]] const outshine::Data::SourceProvider &declaration,
       [[maybe_unused]] std::string_view builtinDataDirectory) const override {
    ++Constructions;
    if (Validations == PreviousValidation) { ++UnvalidatedConstructions; }
    PreviousValidation = Validations;
    return std::make_unique<MetadataSource>(Requests);
  }
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const ExplicitProvider provider;
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
  CHECK(empty.declaration().Providers == requested.Providers,
        "a prepared source permits publication instead of hiding a construction failure");

  Data::ProviderRegistry registry;
  CHECK(registry.registerProvider(provider), "caller owns factory selection");
  Engine configured(std::move(registry));
  CHECK(configured.declare(requested), "host consumes the caller's complete factory registry");
  CHECK(!configured.registerProvider(provider), "registry ownership preserves duplicate detection");
  CHECK(configured.declaration().Providers == requested.Providers,
        "the supplied registry prepares the published source declaration");
  CHECK(provider.Constructions == 2 && provider.UnvalidatedConstructions == 0,
        "each declaration constructs its owned source after factory validation");
  CHECK(provider.Requests == 0, "source preparation does not acquire provider data");
  return Report();
}
