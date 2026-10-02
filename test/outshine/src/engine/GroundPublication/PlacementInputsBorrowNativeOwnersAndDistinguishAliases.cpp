#include "InstancePlacementInputs.h"
#include "Check.h"

#include <array>
#include <memory>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  auto source = std::make_shared<std::array<int, 2>>(std::array{7, 9});
  const std::weak_ptr<std::array<int, 2>> lifetime = source;
  auto first = std::shared_ptr<const int>(source, &(*source)[0]);
  auto second = std::shared_ptr<const int>(source, &(*source)[1]);
  const InstancePlacementInputs input{.Classes = {.Owner = first}};
  const auto repeated = input;
  auto changed = input;
  changed.Classes.Owner = second;
  CHECK(input == repeated && input != changed,
        "unchanged native input matches; a different value under the same owner invalidates it");
  first.reset();
  second.reset();
  source.reset();
  CHECK(lifetime.expired(), "remembering successful empty output does not pin native data");
  auto replacement = std::make_shared<int>(7);
  changed.Classes.Owner = replacement;
  CHECK(input != changed, "a new native producer lifetime cannot reuse a retired input identity");
  auto publication = input;
  publication.Publication = GroundRevision{.ResidentTiles = 1};
  auto wider = publication;
  wider.Publication->ResidentTiles = 2;
  CHECK(publication != wider, "exact placement dependencies include resident coverage changes");
  return Report();
}
