#include <Outshine.h>
#include <generation/Generate.h>
#include "Check.h"

namespace {
class RefusingGenerator final : public outshine::Generators::Generator {
public:
  std::string_view kind() const override { return "refusing-view-publication"; }

  bool make(const outshine::Generators::Request &, outshine::Geometry &) const override {
    return false;
  }
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  RefusingGenerator generator;
  Engine engine;
  engine.offers(generator);
  Scenario::Document original;
  original.Views.push_back({.Id = "original", .Placement = Scenario::CameraPlacement::Local});
  for (int failure = 0; failure < 5; ++failure) {
    CHECK(engine.declare(original).has_value(), "original view catalog activates");
    const auto *previous = engine.declaration().Views.data();
    auto candidate = original;
    candidate.Views[0].Id = "replacement";
    switch (failure) {
      case 0: candidate.Views[0].Id.clear(); break;
      case 1: candidate.Views.push_back(candidate.Views[0]); break;
      case 2: candidate.Played.View = "absent"; break;
      case 3: candidate.Views[0].Placement = Scenario::CameraPlacement::FollowEntity; break;
      default: candidate.Generators.push_back({.Kind = std::string(generator.kind())}); break;
    }
    CHECK(!engine.declare(candidate), "invalid view or late generator failure rejects candidate");
    if (failure < 4) {
      CHECK(engine.declaration().Views.data() == previous,
            "view validation rejects before replacing the owned declaration");
    }
    CHECK(engine.setView("original").has_value(), "failure preserves the previous view catalog");
    CHECK(!engine.setView("replacement"), "failure does not publish candidate view names");
  }
  auto replacement = original;
  replacement.Views[0].Id = "replacement";
  CHECK(engine.declare(replacement).has_value(), "valid retry succeeds");
  CHECK(engine.setView("replacement").has_value() && !engine.setView("original"),
        "successful replacement publishes only its own catalog");
  CHECK(engine.declare({}).has_value(), "empty declaration succeeds");
  CHECK(!engine.setView("replacement"), "successful empty declaration removes the old catalog");
  return Report();
}
