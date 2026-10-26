#include <Engine.h>
#include <SDL3/SDL.h>
#include "Check.h"
#include <string>
#include <expected>
#include <memory>
#include <string_view>
#include <utility>

namespace {
class RefusingProvider final : public outshine::Data::Provider {
public:
  [[nodiscard]] std::string_view kind() const override { return "unavailable"; }

  [[nodiscard]] std::expected<std::unique_ptr<outshine::Data::Source>, std::string>
  make([[maybe_unused]] const outshine::Data::SourceProvider &declaration,
       [[maybe_unused]] std::string_view shippedRoot) const override {
    return std::unexpected("injected unavailable source");
  }
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const RefusingProvider provider;
  constexpr LongitudeLatitude kFocus{.LongitudeDeg = 8, .LatitudeDeg = 47};
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  {
    Engine engine;
    CHECK(engine.registerProvider(provider), "fixture source factory registers before declaration");
    Roots roots;
    roots.Shipped = "data";
    roots.Offline = true;
    CHECK(engine.setRoots(roots).has_value(), "roots are accepted before declaration");
    CHECK(engine.setRenderTarget(Extent{64, 64}).has_value(), "offscreen target configured");
    Scenario::Document initial;
    initial.Room = 4;
    Scenario::Kind kind;
    kind.Name = "actor";
    initial.Kinds.push_back(kind);
    decltype(initial.Instances)::value_type instance;
    instance.Of = "actor";
    instance.Id = "kept";
    initial.Instances.push_back(std::move(instance));
    const auto ready = engine.declare(initial) && engine.assemble();
    CHECK(ready, "initial simulation assembled");
    if (!ready) { return Report(); }
    World::EntityRegistry *const previous = &engine.entities();
    roots.Shipped = "wrong";
    roots.Offline = false;
    const auto changedRoots = engine.setRoots(std::move(roots));
    CHECK(!changedRoots && changedRoots.error().contains("roots"),
          "roots reject a change after declaration");
    CHECK(&engine.entities() == previous, "rejected roots preserve the assembled simulation");
    const auto marker = previous->addEntity();
    CHECK(marker.has_value(), "spare entity capacity remains usable");
    if (!marker) { return Report(); }
    for (const bool populated : {true, false}) {
      Scenario::Document world = populated ? initial : Scenario::Document{};
      world.Ground.Declared = true;
      world.Ground.VegetationEnabled = false;
      world.Ground.Origin.LatitudeDeg = kFocus.LatitudeDeg;
      world.Ground.Origin.LongitudeDeg = kFocus.LongitudeDeg;
      Data::SourceProvider declaration;
      declaration.Kind = "unavailable";
      world.Providers.push_back(std::move(declaration));
      CHECK(engine.declare(world).has_value(), "ground declaration accepted before assembly");
      const auto built = engine.assemble();
      CHECK(!built, "invalid world source fails independently of entity capacity");
      CHECK(!built && built.error().contains("unavailable"),
            "world source failure reaches the caller");
      CHECK(&engine.entities() == previous, "failed world setup preserves simulation ownership");
      if (&engine.entities() != previous) { return Report(); }
      CHECK(previous->alive(*marker), "previous simulation remains usable");
    }
  }
  SDL_Quit();
  return Report();
}
