#include "Generate.h"

#include <vector>

namespace outshine {

struct Makers::Kept {
  std::vector<const Generates *> Held;
};

Makers::Makers() : Kept_(std::make_unique<Kept>()) {}

Makers::~Makers() = default;
Makers::Makers(Makers &&) noexcept = default;
Makers &Makers::operator=(Makers &&) noexcept = default;

bool Makers::offers(const Generates &maker) {
  if (named(maker.kind()) != nullptr) { return false; }
  Kept_->Held.push_back(&maker);
  return true;
}

const Generates *Makers::named(std::string_view kind) const {
  for (const Generates *const stood : Kept_->Held) {
    if (stood->kind() == kind) { return stood; }
  }
  return nullptr;
}

size_t Makers::count() const {
  return Kept_->Held.size();
}

}
