#include "Check.h"
#include "Provider.h"
#include "SourceConfiguration.h"
#include "SourceProviderValidation.h"
#include "SourceSession.h"
#include <array>
#include <cstddef>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace {
using namespace outshine::Data;

SourceProvider Declaration(std::string kind, std::string dataset = {}) {
  SourceProvider declaration;
  declaration.Kind = std::move(kind);
  declaration.Dataset = std::move(dataset);
  return declaration;
}

struct Calls {
  size_t Validated = 0;
  size_t Declared = 0;
  size_t Constructed = 0;
};

class MetadataProvider final : public Provider {
public:
  MetadataProvider(std::string_view name, Calls &calls) : Name_(name), Calls_(calls) {}

  [[nodiscard]] std::string_view kind() const override { return Name_; }

  [[nodiscard]] std::expected<void, std::string>
  validate(const SourceProvider &declaration,
           std::span<const SourceProvider> declared) const override {
    ++Calls_.Validated;
    Calls_.Declared = declared.size();
    if (declaration.Dataset == "blocked") { return std::unexpected("blocked metadata"); }
    return {};
  }

  [[nodiscard]] std::expected<std::unique_ptr<Source>, std::string>
  make([[maybe_unused]] const SourceProvider &declaration,
       [[maybe_unused]] std::string_view root) const override {
    ++Calls_.Constructed;
    return std::unique_ptr<Source>{};
  }

private:
  std::string_view Name_;
  Calls &Calls_;
};

class PermissiveProvider final : public Provider {
public:
  mutable size_t Constructed = 0;

  [[nodiscard]] std::string_view kind() const override { return "fixture.custom"; }

  [[nodiscard]] std::expected<std::unique_ptr<Source>, std::string>
  make([[maybe_unused]] const SourceProvider &declaration,
       [[maybe_unused]] std::string_view root) const override {
    ++Constructed;
    return std::unique_ptr<Source>{};
  }
};
}

int main() {
  using namespace outshine::Test;
  Calls firstCalls;
  Calls secondCalls;
  const MetadataProvider first("fixture.first", firstCalls);
  const MetadataProvider second("fixture.second", secondCalls);
  const PermissiveProvider custom;
  ProviderRegistry registry;
  CHECK(registry.registerProvider(first) && registry.registerProvider(second) &&
            registry.registerProvider(custom),
        "metadata factories register without constructing sources");
  const std::array declared{Declaration("fixture.first"), Declaration("fixture.second", "blocked")};
  const auto valid = ValidateSourceProviders(declared, registry);
  CHECK(!valid && valid.error() == "blocked metadata" && firstCalls.Validated == 1 &&
            secondCalls.Validated == 1 && firstCalls.Declared == declared.size() &&
            secondCalls.Declared == declared.size() && firstCalls.Constructed == 0 &&
            secondCalls.Constructed == 0,
        "all factories receive the full declaration set before any source construction");
  auto directory =
      (std::filesystem::temp_directory_path() / "outshine-source-validation-XXXXXX").string();
  CHECK(mkdtemp(directory.data()) != nullptr, "isolated cache parent exists");
  if (!std::filesystem::is_directory(directory)) { return Report(); }
  const auto cache = std::filesystem::path(directory) / "not-opened";
  const auto session = SourceSession::Open({.Directory = cache.string()}, declared, {}, registry);
  CHECK(!session && session.error() == "blocked metadata" && !std::filesystem::exists(cache) &&
            firstCalls.Constructed == 0 && secondCalls.Constructed == 0,
        "invalid metadata opens neither the source cache nor a source");
  const auto standalone = ConfigureSource(declared.back(), {}, registry);
  CHECK(!standalone && standalone.error() == "blocked metadata" && secondCalls.Constructed == 0,
        "standalone source configuration enforces the same factory boundary");
  auto customDeclaration = Declaration("fixture.custom");
  customDeclaration.PayloadSha256 = "custom-capture";
  customDeclaration.Schema = "private-schema";
  const auto permitted = ConfigureSource(customDeclaration, {}, registry);
  CHECK(!permitted && permitted.error() == "provider factory returned a null source" &&
            custom.Constructed == 1,
        "generic configuration leaves custom metadata to its factory and rejects null results");
  const auto rejected = SourceSession::Open(
      {.Directory = cache.string()}, std::span(&customDeclaration, 1), {}, registry);
  CHECK(!rejected && rejected.error() == "provider factory returned a null source" &&
            !std::filesystem::exists(cache),
        "a rejected factory result opens no source cache");
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  return Report();
}
