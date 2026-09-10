#include "world/EntityRegistry.h"
#include "Check.h"
#include "Outshine.h"
#include <array>
#include <limits>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(TagCatalogue::under(tags::Does, 1) != TagCatalogue::under(tags::Does, 257),
        "distinct capability ordinals do not alias after eight bits");
  CHECK(TagCatalogue::under(tags::Does, 256) != tags::Does,
        "capability 256 does not collapse into the whole family");
  constexpr auto first = TagCatalogue::under(tags::Does, 1);
  constexpr auto last = TagCatalogue::under(tags::Does, 257);
  static_assert(first && last && *first != *last && first->within(tags::Does));
  static_assert(!tags::Does.within(*first) && first->within(*first));
  constexpr uint32_t maximumOrdinal = 16777215;
  constexpr auto at255 = TagCatalogue::under(tags::Does, 255);
  constexpr auto at256 = TagCatalogue::under(tags::Does, 256);
  static_assert(at255 && at256 && *at255 != *at256);
  const auto maximum = TagCatalogue::under(tags::Offers, maximumOrdinal);
  CHECK(maximum && (maximum->value() & maximumOrdinal) == maximumOrdinal &&
            maximum->within(tags::Offers) && !maximum->within(tags::Does),
        "all 24 child bits are preserved within the correct family");
  for (const uint32_t ordinal : {0u, maximumOrdinal + 1, std::numeric_limits<uint32_t>::max()}) {
    const auto invalid = TagCatalogue::under(tags::Does, ordinal);
    CHECK(!invalid && invalid.error() == TagError::InvalidOrdinal,
          "invalid ordinal is an explicit error");
  }
  for (const Tag family : {Tag{}, *first}) {
    const auto invalid = TagCatalogue::under(family, 1);
    CHECK(!invalid && invalid.error() == TagError::InvalidFamily,
          "invalid or nested family is rejected");
  }
  CHECK(!Tag{}.within(tags::Does) && !first->within(Tag{}),
        "invalid tag matches no family or child");
  Scenario::Document scene;
  scene.Room = 4;
  for (size_t i = 0; i < 257; ++i) {
    Scenario::Kind kind;
    kind.Name = "kind-" + std::to_string(i);
    kind.Capabilities.push_back("capability-" + std::to_string(i));
    scene.Kinds.push_back(kind);
  }
  Engine engine;
  const bool built = engine.declare(scene).has_value() && engine.assemble().has_value();
  CHECK(built, "public assembly supports more than 255 distinct capabilities");
  if (built) {
    std::array<Entity, 2> firstMatches{}, lastMatches{};
    auto &registry = engine.entities();
    CHECK(registry.entitiesWithTagAndRole(*first, Role::Body, firstMatches) == 1 &&
              registry.entitiesWithTagAndRole(*last, Role::Body, lastMatches) == 1 &&
              firstMatches[0] != lastMatches[0] && !registry.hasTag(firstMatches[0], *last),
          "public capability queries cannot grant a different capability through ordinal aliasing");
  }
  return Report();
}
