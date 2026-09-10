#include "InputMap.h"
#include "Check.h"
#include <array>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  InputMap map;
  std::string error;
  const std::array initial{Scenario::Binding{.Event = "KeyW", .Action = "forward"},
                           Scenario::Binding{.Event = "MouseLeft", .Action = "fire"}};
  CHECK(map.Build(initial, error), "initial bindings accepted");
  for (const char *last : {"absent", "KeyA"}) {
    const std::array invalid{Scenario::Binding{.Event = "KeyA", .Action = "left"},
                             Scenario::Binding{.Event = last, .Action = "other"}};
    CHECK(!map.Build(invalid, error) && !error.empty(), "invalid suffix rejects full candidate");
    const auto *forward = map.ActionOf("KeyW");
    const auto *fire = map.ActionOf("MouseLeft");
    CHECK(forward && *forward == "forward" && fire && *fire == "fire" &&
              map.ActionOf("KeyA") == nullptr,
          "old bindings survive and candidate prefix never becomes visible");
  }
  const std::array emptyAction{Scenario::Binding{.Event = "KeyA", .Action = ""}};
  CHECK(!map.Build(emptyAction, error) && map.BoundTo("forward") == 1,
        "empty action rejected without replacing previous bindings");
  const std::array shared{Scenario::Binding{.Event = "KeyA", .Action = "turn"},
                          Scenario::Binding{.Event = "KeyD", .Action = "turn"}};
  CHECK(map.Build(shared, error) && map.BoundTo("turn") == 2 && map.BoundTo("Turn") == 0,
        "distinct events share a literal case-sensitive action name");
  const std::array replacement{Scenario::Binding{.Event = "KeyA", .Action = "left"}};
  CHECK(map.Build(replacement, error) && error.empty(), "valid retry clears failure diagnostic");
  CHECK(map.ActionOf("KeyW") == nullptr && map.BoundTo("left") == 1,
        "successful replacement removes old bindings");
  CHECK(map.Build({}, error) && map.BoundTo("left") == 0,
        "empty declaration intentionally clears bindings");
  return Report();
}
