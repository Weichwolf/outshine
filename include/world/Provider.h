#ifndef OUTSHINE_WORLD_PROVIDER_H
#define OUTSHINE_WORLD_PROVIDER_H

#include <cstddef>
#include <expected>
#include <memory>
#include <string>
#include <string_view>

#include "SourceProvider.h"
#include "data/Source.h"

namespace outshine::Data {

/// Borrowed factory for configured world-data sources. Its stable address must outlive
/// registrations and the Engine. Concurrent configuration may run on setup/IO workers;
/// make performs no IO and synchronizes mutable state. Returned sources perform
/// bounded work on IO workers instead.
class Provider {
public:
  /// Release the factory after all borrowing engines have been destroyed.
  virtual ~Provider() = default;
  /// Factory identity cannot be copied implicitly.
  Provider(const Provider &) = delete;
  /// Factory identity cannot be replaced implicitly.
  Provider &operator=(const Provider &) = delete;

  /// Return the case-sensitive declaration kind, readable throughout registration.
  /// @return Nonempty name; the registry copies it and never queries it again.
  [[nodiscard]] virtual std::string_view kind() const = 0;
  /// Validate configuration and create one independently owned source without IO.
  /// @param declaration Borrowed settings; source copies any retained values.
  /// @param shippedRoot Borrowed shipped-data directory; no cache ownership is transferred.
  /// @return Owned configured source, or owned diagnostic. May allocate; no blocking work.
  [[nodiscard]] virtual std::expected<std::unique_ptr<Source>, std::string>
  make(const SourceProvider &declaration, std::string_view shippedRoot) const = 0;

protected:
  /// Construct the interface without allocation.
  Provider() = default;
};

/// Catalogue owning names and borrowing Provider factories. Concurrent registration
/// and lookups are safe; serialize destruction/move with all access. Registration does
/// not create sources or perform IO. Moved-from objects support destruction/assignment.
class ProviderRegistry {
public:
  /// Registration failure preserving every previous entry.
  enum class RegistrationError {
    EmptyKind,    ///< Factory returned an empty kind.
    DuplicateKind ///< An existing factory owns the exact name.
  };

  /// Construct an empty catalogue; may allocate.
  ProviderRegistry();
  /// Release names and storage; borrowed factories remain externally owned.
  ~ProviderRegistry();
  /// Transfer catalogue storage without moving factories.
  /// @param other Source left usable for destruction or assignment only.
  ProviderRegistry(ProviderRegistry &&other) noexcept;
  /// Release old names and transfer the catalogue without moving factories.
  /// @param other Source left usable for destruction or assignment only.
  /// @return This registry.
  ProviderRegistry &operator=(ProviderRegistry &&other) noexcept;
  /// Catalogue ownership is not implicitly copied.
  ProviderRegistry(const ProviderRegistry &) = delete;
  /// Catalogue ownership is not implicitly replaced.
  ProviderRegistry &operator=(const ProviderRegistry &) = delete;

  /// Copy a kind name and borrow its factory at a stable address; linear setup work.
  /// @param provider Factory that outlives this registration.
  /// @return Success, empty-kind or duplicate-kind failure; may allocate on success.
  [[nodiscard]] std::expected<void, RegistrationError> registerProvider(const Provider &provider);
  /// Resolve a copied kind without calling factory methods or allocating.
  /// @param kind Borrowed lookup key; exact case-sensitive comparison.
  /// @return Borrowed factory or nullptr; linear in registrations and name lengths.
  [[nodiscard]] const Provider *named(std::string_view kind) const;
  /// Count registered names without allocating.
  /// @return Number of factories; constant-time.
  [[nodiscard]] size_t count() const;

private:
  struct Kept;
  std::unique_ptr<Kept> Kept_;
};

}
#endif
