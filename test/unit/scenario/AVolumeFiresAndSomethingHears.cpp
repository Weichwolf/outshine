#include <cstdio>
#include <cstdlib>
#include <new>
#include <cstring>
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

[[nodiscard]] bool Stood(const char *text, TriggerField &field, std::string &error) {
  Scenario declared;
  if (!ReadScenario(text, std::strlen(text), declared, error)) { return false; }
  return field.Build(declared.Volumes, declared.Events, error);
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  std::string error;
  TriggerField field;
  const bool up = Stood(
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
      "</volumes></scenario>",
      field, error);
  if (!up) { std::printf("REFUSED %s\n", error.c_str()); }
  CHECK(up, "three declared doors stand up over three declared events");
  if (!up) { return Report(); }

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
    TriggerField bad;
    CHECK(!Stood("<scenario name=\"t\"><events><event name=\"e\"/></events><volumes>"
                 "<volume id=\"v\" fires=\"e\" when=\"sometimes\"/></volumes></scenario>",
                 bad, error) &&
              error.find("no fourth") != std::string::npos,
          "a fourth when does not exist");
    CHECK(!Stood("<scenario name=\"t\"><events><event name=\"e\"/></events><volumes>"
                 "<volume id=\"v\" fires=\"boom\" when=\"enter\"/></volumes></scenario>",
                 bad, error) &&
              error.find("boom") != std::string::npos && error.find("e") != std::string::npos,
          "a volume firing an undeclared event refuses naming it and the list");
    CHECK(!Stood("<scenario name=\"t\"><events><event name=\"e\"/></events><volumes>"
                 "<volume id=\"v\" fires=\"e\" when=\"dwell\"/></volumes></scenario>",
                 bad, error) &&
              error.find("dwellS") != std::string::npos,
          "a dwell without a duration refuses -- an enter wearing a costume");
  }

  Covers("III.11 a volume fires an event and something hears it: enter, exit and dwell and "
         "no fourth, listeners hold their fields at stand-up, firing is bounded and "
         "allocation-free after build, and an unheard event is counted (board:1488)");
  return Report();
}
