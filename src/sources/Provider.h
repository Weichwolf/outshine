#ifndef OUTSHINE_SOURCES_PROVIDER_H
#define OUTSHINE_SOURCES_PROVIDER_H

#include <cstddef>
#include <expected>
#include <memory>
#include <span>
#include <string>
#include <string_view>

#include "SourceProvider.h"
#include "Source.h"

namespace outshine::Data {

// Factories outlive their engines; validate and make perform no IO.
class Provider {
public:
  virtual ~Provider() = default;

  Provider(const Provider &) = delete;

  Provider &operator=(const Provider &) = delete;

  [[nodiscard]] virtual std::string_view kind() const = 0;

  [[nodiscard]] virtual std::expected<void, std::string>
  validate([[maybe_unused]] const SourceProvider &declaration,
           [[maybe_unused]] std::span<const SourceProvider> declared) const {
    return {};
  }

  [[nodiscard]] virtual std::expected<std::unique_ptr<Source>, std::string>
  make(const SourceProvider &declaration, std::string_view shippedRoot) const = 0;

protected:
  Provider() = default;
};

class ProviderRegistry {
public:
  enum class RegistrationError { EmptyKind, DuplicateKind };

  ProviderRegistry();

  ~ProviderRegistry();

  ProviderRegistry(ProviderRegistry &&other) noexcept;

  ProviderRegistry &operator=(ProviderRegistry &&other) noexcept;

  ProviderRegistry(const ProviderRegistry &) = delete;

  ProviderRegistry &operator=(const ProviderRegistry &) = delete;

  [[nodiscard]] std::expected<void, RegistrationError> registerProvider(const Provider &provider);

  [[nodiscard]] const Provider *named(std::string_view kind) const;

  [[nodiscard]] size_t count() const;

private:
  struct Kept;
  std::unique_ptr<Kept> Kept_;
};

}
#endif
