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

bool Makers::Offers(const Generates &maker) {
  if (Named(maker.Kind()) != nullptr) { return false; }
  Kept_->Held.push_back(&maker);
  return true;
}

const Generates *Makers::Named(std::string_view kind) const {
  for (const Generates *const stood : Kept_->Held) {
    if (stood->Kind() == kind) { return stood; }
  }
  return nullptr;
}

size_t Makers::Count() const { return Kept_->Held.size(); }

}
