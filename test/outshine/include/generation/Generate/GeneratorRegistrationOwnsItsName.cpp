#include <generation/Generate.h>
#include <string>
#include <utility>
#include "Check.h"

namespace {
class NamedGenerator final : public outshine::Generators::Generator {
public:
  std::string Name;
  mutable size_t Calls = 0;

  [[nodiscard]] std::string_view kind() const override {
    ++Calls;
    return Name;
  }

  [[nodiscard]] Product make(const outshine::Generators::Request &) const override {
    return std::unexpected("unused");
  }
};
}

int main() {
  using namespace outshine::Generators;
  using namespace outshine::Test;
  NamedGenerator first;
  NamedGenerator duplicate;
  Registry registry;
  const auto empty = registry.offers(first);
  CHECK(!empty && empty.error() == Registry::RegistrationError::EmptyKind && registry.count() == 0,
        "empty name cannot enter catalogue");
  first.Name = "structures";
  CHECK(registry.offers(first).has_value() && registry.count() == 1, "nonempty name registers");
  const size_t calls = first.Calls;
  first.Name = "changed";
  CHECK(registry.named("structures") == &first && registry.named("changed") == nullptr,
        "registration owns its original name");
  CHECK(first.Calls == calls, "lookup does not execute generator callbacks");
  duplicate.Name = "structures";
  const auto duplicateResult = registry.offers(duplicate);
  CHECK(!duplicateResult && duplicateResult.error() == Registry::RegistrationError::DuplicateKind &&
            registry.count() == 1 && registry.named("structures") == &first,
        "duplicate cannot replace existing registration after the provider renames itself");
  CHECK(registry.named("Structures") == nullptr && registry.named("") == nullptr,
        "lookup is case sensitive and unknown names have no fallback");
  Registry moved(std::move(registry));
  CHECK(moved.named("structures") == &first && moved.count() == 1,
        "move transfers names and preserves borrowed generator addresses");
  CHECK(nameOf(Shipped::Structures) == "structures", "known built-in resolves its declared name");
  for (const auto invalid : {Shipped::kCount, static_cast<Shipped>(255)}) {
    CHECK(nameOf(invalid).empty(), "invalid catalogue values are rejected without array access");
  }
  return Report();
}
