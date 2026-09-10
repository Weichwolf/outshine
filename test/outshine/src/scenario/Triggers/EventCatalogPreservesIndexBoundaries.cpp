#include "Triggers.h"
#include "Check.h"
#include <limits>
#include <string>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const size_t capacity = static_cast<size_t>(std::numeric_limits<uint16_t>::max()) + 1;
  std::vector<Scenario::Event> events;
  events.reserve(capacity + 1);
  for (size_t at = 0; at <= capacity; ++at) {
    events.push_back({.Name = "event-" + std::to_string(at)});
  }
  std::vector<Scenario::Volume> volumes{
      {.Id = "first", .Shape = "box", .Fires = events.front().Name, .When = "enter"}};
  volumes.push_back(volumes.front());
  volumes.back().Id = "last";
  volumes.back().Fires = events[capacity - 1].Name;
  events[capacity - 1].Carries = {"speed"};
  auto field = TriggerField::Stand(volumes, std::span(events).first(capacity));
  CHECK(field.has_value(), "all uint16 indices remain usable without a count sentinel");
  if (field) {
    const std::string_view read = "speed";
    std::string error;
    CHECK(field->Listen(events[capacity - 1].Name, std::span(&read, 1), error),
          "listener field lookup reaches the final event slot");
    field->Probe({.Index = 1, .Generation = 1}, {}, 0);
    CHECK(field->Unheard(events.front().Name) == 1 &&
              field->Unheard(events[capacity - 1].Name) == 0,
          "listener state and unhandled counters use the correct boundary slots");
    const auto fired = field->Drain();
    CHECK(fired.size() == 2, "both boundary-index volumes emit");
    if (fired.size() == 2) {
      CHECK(fired[0].Event == 0 && fired[1].Event == capacity - 1,
            "first and last event indices do not alias");
      const auto *name = field->EventNamed(fired[1].Event);
      CHECK(name && *name == events[capacity - 1].Name, "last event name is resolved exactly");
    }
  }
  volumes.resize(1);
  volumes[0].Fires = events.back().Name;
  CHECK(!TriggerField::Stand(volumes, events), "one event beyond capacity cannot wrap its target");
  CHECK(!TriggerField::Stand({}, events), "oversized catalogs are rejected even without volumes");
  const std::vector<Scenario::Event> duplicate{{.Name = "same"}, {.Name = "same"}};
  CHECK(!TriggerField::Stand({}, duplicate), "duplicate event names are rejected");
  const std::vector<Scenario::Event> unnamed(1);
  CHECK(!TriggerField::Stand({}, unnamed), "empty event names are rejected");
  volumes[0].Fires = "absent";
  CHECK(!TriggerField::Stand(volumes, std::span(events).first(1)), "unknown event does not bind");
  CHECK(TriggerField::Stand({}, {}).has_value(), "an empty trigger field remains valid");
  return Report();
}
