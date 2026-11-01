#include <Engine.h>
#include <Generator.h>
#include "Check.h"
#include <expected>
#include <span>
#include <string>
#include <string_view>

namespace {
class Receiver final : public outshine::Host {
public:
  std::string Last;

  bool calls(std::string_view name, std::span<const outshine::Argument> /*args*/) override {
    Last = name;
    return true;
  }
};

class RefusingGenerator final : public outshine::Generators::Generator {
public:
  [[nodiscard]] std::string_view kind() const override { return "refusing"; }

  [[nodiscard]] Product make(const outshine::Generators::Request & /*asked*/) const override {
    return std::unexpected("refused");
  }
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Receiver receiver;
  const RefusingGenerator generator;
  Scenario::Document original;
  original.Input = {{.Event = "KeyW", .Action = "original"}};
  SDL_Event event{};
  event.type = SDL_EVENT_KEY_DOWN;
  event.key.key = SDLK_W;
  for (int failure = 0; failure < 4; ++failure) {
    Engine engine;
    engine.setInputHost(&receiver);
    CHECK(engine.registerGenerator(generator), "fixture generator registers");
    auto candidate = original;
    candidate.Input[0].Action = "replacement";
    switch (failure) {
      case 0: candidate.Input.push_back({.Event = "absent", .Action = "invalid"}); break;
      case 1: candidate.Input.push_back({.Event = "KeyW", .Action = "duplicate"}); break;
      case 2:
        candidate.Views.push_back({.Id = "valid",
                                   .Sees = {},
                                   .Geographic = {},
                                   .Route = {},
                                   .Viewport = {},
                                   .In = {},
                                   .Follows = {},
                                   .Person = {},
                                   .OffsetM = {}});
        candidate.Played.View = "absent";
        break;
      default: candidate.Generators.push_back({.Kind = "refusing", .Parameters = {}}); break;
    }
    CHECK(!engine.loadScenario(candidate), "invalid candidate is refused");
  }
  Engine valid;
  valid.setInputHost(&receiver);
  CHECK(valid.loadScenario(original), "valid input configuration starts");
  CHECK(valid.handleEvent(event).value_or(false) && receiver.Last == "original",
        "startup activates its declared bindings");
  Engine empty;
  CHECK(empty.loadScenario({}), "empty scenario starts independently");
  const auto unbound = empty.handleEvent(event);
  CHECK(unbound && !*unbound, "an empty startup has no input bindings");
  return Report();
}
