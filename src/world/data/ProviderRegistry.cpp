#include <world/Provider.h>

#include <expected>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <mutex>
#include <ranges>
#include <algorithm>

namespace outshine::Data {

struct ProviderRegistry::Kept {
  struct Entry {
    std::string Name;
    const Provider *Factory = nullptr;
  };

  std::vector<Entry> Entries;
  std::mutex Mutex;
};

ProviderRegistry::ProviderRegistry() : Kept_(std::make_unique<Kept>()) {}

ProviderRegistry::~ProviderRegistry() = default;
ProviderRegistry::ProviderRegistry(ProviderRegistry &&) noexcept = default;
ProviderRegistry &ProviderRegistry::operator=(ProviderRegistry &&) noexcept = default;

std::expected<void, ProviderRegistry::RegistrationError>
ProviderRegistry::registerProvider(const Provider &provider) {
  const auto name = provider.kind();
  if (name.empty()) { return std::unexpected(RegistrationError::EmptyKind); }
  const std::scoped_lock lock(Kept_->Mutex);
  if (std::ranges::any_of(Kept_->Entries,
                          [name](const auto &entry) { return entry.Name == name; })) {
    return std::unexpected(RegistrationError::DuplicateKind);
  }
  Kept_->Entries.push_back({.Name = std::string(name), .Factory = &provider});
  return {};
}

const Provider *ProviderRegistry::named(std::string_view kind) const {
  const std::scoped_lock lock(Kept_->Mutex);
  for (const auto &entry : Kept_->Entries) {
    if (entry.Name == kind) { return entry.Factory; }
  }
  return nullptr;
}

size_t ProviderRegistry::count() const {
  const std::scoped_lock lock(Kept_->Mutex);
  return Kept_->Entries.size();
}

}
