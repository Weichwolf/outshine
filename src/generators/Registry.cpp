#include "generate/Generate.h"

#include <memory>
#include <string_view>
#include <string>
#include <cstddef>
#include <vector>

namespace outshine::Generators {

struct Registry::Kept {
  struct Entry {
    std::string Name;
    const Generator *Maker = nullptr;
  };

  std::vector<Entry> Held;
};

Registry::Registry() : Kept_(std::make_unique<Kept>()) {}

Registry::~Registry() = default;
Registry::Registry(Registry &&) noexcept = default;
Registry &Registry::operator=(Registry &&) noexcept = default;

bool Registry::offers(const Generator &maker) {
  const std::string_view name = maker.kind();
  if (name.empty() || named(name) != nullptr) { return false; }
  Kept_->Held.push_back({.Name = std::string(name), .Maker = &maker});
  return true;
}

const Generator *Registry::named(std::string_view kind) const {
  for (const Kept::Entry &entry : Kept_->Held) {
    if (entry.Name == kind) { return entry.Maker; }
  }
  return nullptr;
}

size_t Registry::count() const {
  return Kept_->Held.size();
}

}
