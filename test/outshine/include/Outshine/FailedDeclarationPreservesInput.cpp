#include <Outshine.h>
#include <generation/Generate.h>
#include "Check.h"
#include <string>

namespace {
class Receiver final : public outshine::Host {
public:
  std::string Last;

  bool calls(std::string_view name, std::span<const outshine::Argument>) override {
    Last = name;
    return true;
  }
};

class RefusingGenerator final : public outshine::Generators::Generator {
public:
  std::string_view kind() const override { return "refusing"; }

  Product make(const outshine::Generators::Request &) const override {
    return std::unexpected("refused");
  }
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Receiver receiver;
  RefusingGenerator generator;
  Engine engine;
  engine.setInputHost(&receiver);
  CHECK(engine.registerGenerator(generator), "fixture generator registers");
  Scenario::Document original;
  original.Input = {{.Event = "KeyW", .Action = "original"}};
  SDL_Event event{};
  event.type = SDL_EVENT_KEY_DOWN;
  event.key.key = SDLK_W;
  for (int failure = 0; failure < 4; ++failure) {
    CHECK(engine.declare(original).has_value(), "original input activates");
    auto candidate = original;
    candidate.Input[0].Action = "replacement";
    switch (failure) {
      case 0: candidate.Input.push_back({.Event = "absent", .Action = "invalid"}); break;
      case 1: candidate.Input.push_back({.Event = "KeyW", .Action = "duplicate"}); break;
      case 2:
        candidate.Views.push_back({.Id = "valid"});
        candidate.Played.View = "absent";
        break;
      default: candidate.Generators.push_back({.Kind = "refusing"}); break;
    }
    CHECK(!engine.declare(candidate), "invalid candidate is refused");
    CHECK(engine.declaration().Input.size() == 1 &&
              engine.declaration().Input.front().Event == "KeyW" &&
              engine.declaration().Input.front().Action == "original" &&
              engine.declaration().Generators.empty(),
          "a rejected declaration retains the complete previous declaration");
    receiver.Last.clear();
    const auto handled = engine.handleEvent(event);
    CHECK(handled && *handled && receiver.Last == "original",
          "early and late declaration failures retain original input action");
  }
  auto replacement = original;
  replacement.Input[0].Action = "replacement";
  CHECK(engine.declare(replacement).has_value(), "valid replacement recovers");
  CHECK(engine.handleEvent(event).value_or(false) && receiver.Last == "replacement",
        "successful declaration activates its own bindings");
  CHECK(engine.declare({}).has_value(), "empty declaration succeeds");
  const auto removed = engine.handleEvent(event);
  CHECK(removed && !*removed, "successful empty declaration removes bindings");
  return Report();
}
