#include <Engine.h>
#include <Generator.h>
#include <expected>
#include <string_view>
#include "Check.h"

namespace {
class RefusingGenerator final : public outshine::Generators::Generator {
public:
  [[nodiscard]] std::string_view kind() const override { return "refusing-view-publication"; }

  [[nodiscard]] Product make(const outshine::Generators::Request & /*asked*/) const override {
    return std::unexpected("refused");
  }
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const RefusingGenerator generator;
  Scenario::Document original;
  original.Views.push_back({.Id = "original",
                            .Sees = {},
                            .Placement = Scenario::CameraPlacement::Local,
                            .Geographic = {},
                            .Route = {},
                            .Viewport = {},
                            .In = {},
                            .Follows = {},
                            .Person = {},
                            .OffsetM = {}});
  for (int failure = 0; failure < 5; ++failure) {
    Engine engine;
    CHECK(engine.registerGenerator(generator), "fixture generator registers");
    auto candidate = original;
    candidate.Views[0].Id = "replacement";
    switch (failure) {
      case 0: candidate.Views[0].Id.clear(); break;
      case 1: candidate.Views.push_back(candidate.Views[0]); break;
      case 2: candidate.Played.View = "absent"; break;
      case 3: candidate.Views[0].Placement = Scenario::CameraPlacement::FollowEntity; break;
      default:
        candidate.Generators.push_back({.Kind = std::string(generator.kind()), .Parameters = {}});
        break;
    }
    CHECK(!engine.loadScenario(candidate),
          "invalid view or late generator failure rejects candidate");
  }
  auto replacement = original;
  replacement.Views[0].Id = "replacement";
  Engine valid;
  CHECK(valid.loadScenario(replacement).has_value(), "valid view startup succeeds");
  CHECK(valid.setView("replacement").has_value() && !valid.setView("original"),
        "startup publishes only its declared view catalog");
  Engine empty;
  CHECK(empty.loadScenario({}).has_value(), "empty startup succeeds");
  CHECK(!empty.setView("replacement"), "an empty startup has no view catalog");
  return Report();
}
