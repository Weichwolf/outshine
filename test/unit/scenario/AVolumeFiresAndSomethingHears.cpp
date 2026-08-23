#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <cstring>
#include <expected>
#include <utility>
#include <string>
#include <vector>

#include "Check.h"

#include "ScenarioRead.h"
#include "Triggers.h"

using outshine::ReadScenario;
using outshine::Scenario;
using outshine::TriggerField;

namespace {
size_t gAllocations = 0;
}

void *operator new(size_t bytes) {
  ++gAllocations;
  void *held = std::malloc(bytes ? bytes : 1);
  if (held == nullptr) { throw std::bad_alloc(); }
  return held;
}
void operator delete(void *held) noexcept { std::free(held); }
void operator delete(void *held, size_t) noexcept { std::free(held); }

namespace {

[[nodiscard]] std::expected<TriggerField, std::string> Stood(const char *text) {
  Scenario declared;
  std::string refusal;
  if (!ReadScenario(text, std::strlen(text), declared, refusal)) {
    return std::unexpected(refusal);
  }
  return TriggerField::Stand(declared.Volumes, declared.Events);
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  std::string error;
  auto stood = Stood(
      "<scenario name=\"doors\">"
      "<events><event name=\"crossed\"><carries what=\"body\"/></event>"
      "<event name=\"lingered\"/><event name=\"left\"/></events>"
      "<volumes>"
      "<volume id=\"gate\" shape=\"box\" x=\"10\" y=\"0\" z=\"0\" extentX=\"2\" "
      "extentY=\"2\" extentZ=\"2\" fires=\"crossed\" when=\"enter\"/>"
      "<volume id=\"camp\" shape=\"sphere\" x=\"50\" y=\"0\" z=\"0\" extentX=\"5\" "
      "fires=\"lingered\" when=\"dwell\" dwellS=\"2\"/>"
      "<volume id=\"door\" shape=\"box\" x=\"10\" y=\"0\" z=\"0\" extentX=\"2\" "
      "extentY=\"2\" extentZ=\"2\" fires=\"left\" when=\"exit\"/>"
      "</volumes></scenario>");
  if (!stood) { std::printf("REFUSED %s\n", stood.error().c_str()); }
  CHECK(stood.has_value(), "three declared doors stand up over three declared events");
  if (!stood) { return Report(); }
  TriggerField field = *std::move(stood);

  std::vector<std::string_view> reads{"body"};
  CHECK(field.Listen("crossed", reads, error),
        "a listener reading a carried field stands");
  CHECK(!field.Listen("crossed", std::vector<std::string_view>{"speed"}, error) &&
            error.find("speed") != std::string::npos,
        "**A LISTENER READING WHAT THE EVENT DOES NOT CARRY REFUSES AT STAND-UP**, naming "
        "the field -- never a null at run time");

  const double outside[3] = {0.0, 0.0, 0.0};
  const double inGate[3] = {10.5, 0.5, 0.0};
  const double inCamp[3] = {52.0, 0.0, 0.0};
  field.Probe(7, outside, 0.0);
  CHECK(field.Drain().empty(), "outside every door nothing fires");
  field.Probe(7, inGate, 0.1);
  auto fired = field.Drain();
  CHECK(fired.size() == 1 && *field.EventNamed(fired[0].Event) == "crossed" &&
            fired[0].Body == 7,
        "**ENTER FIRES ON THE CROSSING** with the body that crossed");
  field.Probe(7, inGate, 0.2);
  CHECK(field.Drain().empty(), "standing inside refires nothing");
  field.Probe(7, outside, 0.3);
  fired = field.Drain();
  CHECK(fired.size() == 1 && *field.EventNamed(fired[0].Event) == "left",
        "**EXIT FIRES ON THE WAY OUT** -- the box the gate shares fired its own event");

  field.Probe(7, inCamp, 1.0);
  field.Probe(7, inCamp, 2.0);
  CHECK(field.Drain().empty(), "a dwell holds its tongue before its declared seconds");
  field.Probe(7, inCamp, 3.5);
  fired = field.Drain();
  CHECK(fired.size() == 1 && *field.EventNamed(fired[0].Event) == "lingered",
        "**DWELL FIRES AFTER ITS DECLARED SECONDS** -- and only once");
  field.Probe(7, inCamp, 9.0);
  CHECK(field.Drain().empty(), "and stays quiet while the body lingers on");

  {
    const size_t before = gAllocations;
    field.Probe(7, inGate, 10.0);
    field.Probe(7, outside, 10.1);
    (void)field.Drain();
    CHECK(gAllocations == before,
          "**FIRING TAKES NOTHING FROM THE ALLOCATOR**: an enter, an exit and a drain run "
          "on the pools the build reserved -- zero allocations, counted at the global "
          "operator new");
  }

  CHECK(field.Unheard("lingered") == 1 && field.Unheard("crossed") == 0,
        "**AN EVENT NOBODY LISTENS TO IS COUNTED** -- the scenario can see its trigger "
        "reaches nothing");

  {
    const auto fourth =
        Stood("<scenario name=\"t\"><events><event name=\"e\"/></events><volumes>"
              "<volume id=\"v\" fires=\"e\" when=\"sometimes\"/></volumes></scenario>");
    CHECK(!fourth && fourth.error().find("no fourth") != std::string::npos,
          "a fourth when does not exist");

    const auto unheard =
        Stood("<scenario name=\"t\"><events><event name=\"e\"/></events><volumes>"
              "<volume id=\"v\" fires=\"boom\" when=\"enter\"/></volumes></scenario>");
    CHECK(!unheard && unheard.error().find("boom") != std::string::npos &&
              unheard.error().find("declares: e") != std::string::npos,
          "a volume firing an undeclared event refuses naming it and the list");

    const auto costume =
        Stood("<scenario name=\"t\"><events><event name=\"e\"/></events><volumes>"
              "<volume id=\"v\" fires=\"e\" when=\"dwell\"/></volumes></scenario>");
    CHECK(!costume && costume.error().find("dwellS") != std::string::npos,
          "a dwell without a duration refuses -- an enter wearing a costume");
  }

  {
    // the probe's cost is its OWN door's few standings, never every (body, door) pair:
    // 200 bodies inside 256 doors cost 66.68 ms a tick when this was found -- four frames
    // for one tick, at the pool's own declared bounds (board:1759)
    std::string many = "<scenario name=\"crowd\"><events><event name=\"e\"/></events><volumes>";
    for (int at = 0; at < 256; ++at) {
      many += "<volume id=\"v" + std::to_string(at) + "\" x=\"" + std::to_string(at * 100) +
              "\" y=\"0\" z=\"0\" extentX=\"50\" extentY=\"50\" extentZ=\"50\" "
              "fires=\"e\" when=\"enter\"/>";
    }
    many += "</volumes></scenario>";
    Scenario declared;
    CHECK(ReadScenario(many.c_str(), many.size(), declared, error),
          "a scenario of 256 doors reads");
    auto raised = TriggerField::Stand(declared.Volumes, declared.Events);
    CHECK(raised.has_value(), "a scenario of 256 doors stands");
    if (!raised) { return Report(); }
    TriggerField crowd = *std::move(raised);

    const size_t before = gAllocations;
    const auto from = std::chrono::steady_clock::now();
    for (int tick = 0; tick < 60; ++tick) {
      for (int body = 0; body < 200; ++body) {
        const double where[3] = {(double)(body % 128) * 100.0, 0.0, 0.0};
        crowd.Probe((uint32_t)body, where, (double)tick * 0.016);
      }
      (void)crowd.Drain();
    }
    const double perTick = std::chrono::duration<double, std::milli>(
                               std::chrono::steady_clock::now() - from).count() / 60.0;
    Note("200 bodies inside 256 doors", perTick, "ms per tick");
    CHECK(perTick < 16.67 / 4.0,
          "**A TRIGGER PROBE COSTS THE DOORS AND NOT THE STANDINGS**: a quarter of one "
          "frame for the whole crowd, where the pair scan cost four FRAMES (board:1759)");
    CHECK(gAllocations == before,
          "and the crowd's ticks still take nothing from the allocator");
    CHECK(crowd.Unseated() == 0,
          "nothing went unseated at this population -- and a body that cannot be seated is "
          "COUNTED rather than firing Enter every tick and never Exit");
  }

  Covers("III.11 a volume fires an event and something hears it: enter, exit and dwell and "
         "no fourth, listeners hold their fields at stand-up, firing is bounded and "
         "allocation-free after build, and an unheard event is counted (board:1488)");
  return Report();
}
