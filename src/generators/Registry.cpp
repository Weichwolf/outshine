#include "generate/Generate.h"

#include <memory>
#include <string_view>
#include <cstddef>
#include <vector>

namespace outshine::Generators {

struct Registry::Kept {
  std::vector<const Generator *> Held;
};

Registry::Registry() : Kept_(std::make_unique<Kept>()) {}

Registry::~Registry() = default;
Registry::Registry(Registry &&) noexcept = default;
Registry &Registry::operator=(Registry &&) noexcept = default;

bool Registry::offers(const Generator &maker) {
  if (named(maker.kind()) != nullptr) { return false; }
  Kept_->Held.push_back(&maker);
  return true;
}

const Generator *Registry::named(std::string_view kind) const {
  for (const Generator *const stood : Kept_->Held) {
    if (stood->kind() == kind) { return stood; }
  }
  return nullptr;
}

size_t Registry::count() const {
  return Kept_->Held.size();
}

} // namespace outshine::Generators
