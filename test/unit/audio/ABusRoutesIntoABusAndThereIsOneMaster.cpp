#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "Check.h"

#include "BusGraph.h"
#include "ScenarioRead.h"

using outshine::ReadScenario;
using outshine::Scenario;
using outshine::Audio::BusGraph;

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

[[nodiscard]] bool Stood(const char *text, BusGraph &graph, std::string &error) {
  Scenario declared;
  if (!ReadScenario(text, std::strlen(text), declared, error)) { return false; }
  return graph.Build(declared.Buses, declared.Sounds, error);
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  std::string error;
  BusGraph graph;
  const bool up = Stood(
      "<scenario name=\"heard\"><audio>"
      "<bus id=\"master\" gainDb=\"0\"/>"
      "<bus id=\"music\" into=\"master\" gainDb=\"-6\"/>"
      "<bus id=\"world\" into=\"master\" gainDb=\"0\"/>"
      "<sound id=\"radio\" uri=\"radio.ogg\" bus=\"music\" loops=\"true\" gainDb=\"0\"/>"
      "<sound id=\"step\" uri=\"step.ogg\" bus=\"world\" positional=\"true\" falloffM=\"20\"/>"
      "</audio></scenario>",
      graph, error);
  if (!up) { std::printf("REFUSED %s\n", error.c_str()); }
  CHECK(up, "a declared mix stands up once");
  if (!up) { return Report(); }

  CHECK(graph.Master() == "master" && graph.BusCount() == 3 && graph.SoundCount() == 2,
        "**A BUS ROUTES INTO A BUS AND THERE IS ONE MASTER** -- the bus that routes into "
        "nothing is it, answerable by name");

  const double music = graph.GainOf("radio");
  Note("the radio's gain at the master", music, "linear");
  CHECK(std::fabs(music - std::pow(10.0, -6.0 / 20.0)) < 1e-9,
        "**A SOURCE ARRIVES CARRYING EVERY BUS ON ITS ROUTE**: -6 dB of music is 0.501 "
        "linear at the master -- ducking music under dialogue is a bus edit, and the "
        "engine never learns what music is");

  const double at0[3] = {0, 0, 0};
  const double at20[3] = {20, 0, 0};
  const double near = graph.GainAt("step", at0, at0);
  const double far = graph.GainAt("step", at20, at0);
  Note("a footstep at the ear", near, "linear");
  Note("the same footstep twenty metres off", far, "linear");
  CHECK(std::fabs(near - 1.0) < 1e-9 && std::fabs(far - 0.5) < 1e-9,
        "**A POSITIONAL SOURCE ATTENUATES BY ITS DECLARED FALLOFF**: half as loud at "
        "exactly the declared twenty metres, against the one listener");
  CHECK(std::fabs(graph.GainAt("radio", at20, at0) - music) < 1e-9,
        "and a non-positional source ignores the distance, as declared");

  {
    const size_t before = gAllocations;
    double sum = 0.0;
    for (int voice = 0; voice < 512; ++voice) {
      const double where[3] = {(double)voice, 0.0, 0.0};
      sum += graph.GainAt("step", where, at0);
    }
    CHECK(sum > 0.0 && gAllocations == before,
          "**THE MIX TAKES NOTHING FROM THE ALLOCATOR**: five hundred voice gains walk the "
          "graph with zero allocations -- the audio callback's deadline is the buffer's, "
          "and a miss is a click every listener hears");
  }

  CHECK(graph.Play("radio", error) && graph.Playing() == 1,
        "**A SOUND IS PLAYED BY ID AND THE ENGINE ANSWERS WHETHER IT STARTED**");
  CHECK(!graph.Play("thunder", error) && error.find("thunder") != std::string::npos &&
            error.find("never a silence") != std::string::npos,
        "**A MISSING SOUND IS A NAMED REFUSAL AND NEVER A SILENCE** -- a silence is what a "
        "working sound also sounds like when nothing is happening");

  {
    BusGraph bad;
    CHECK(!Stood("<scenario name=\"t\"><audio><bus id=\"m\"/>"
                 "<bus id=\"a\" into=\"b\"/><bus id=\"b\" into=\"a\"/>"
                 "</audio></scenario>",
                 bad, error) &&
              error.find("cycle") != std::string::npos,
          "a routing cycle BESIDE a master refuses naming it -- a cycle on the audio "
          "thread is a hang between two buffers");
    CHECK(!Stood("<scenario name=\"t\"><audio>"
                 "<bus id=\"a\" into=\"b\"/><bus id=\"b\" into=\"a\"/>"
                 "</audio></scenario>",
                 bad, error) &&
              error.find("no master") != std::string::npos,
          "and a graph that is ONLY a cycle refuses for the master it never reaches");
    CHECK(!Stood("<scenario name=\"t\"><audio><bus id=\"a\"/><bus id=\"b\"/>"
                 "</audio></scenario>",
                 bad, error) &&
              error.find("ONE master") != std::string::npos,
          "two masters refuse -- a mix has one");
    CHECK(!Stood("<scenario name=\"t\"><audio><bus id=\"m\"/>"
                 "<sound id=\"s\" uri=\"s.ogg\" bus=\"gone\"/></audio></scenario>",
                 bad, error) &&
              error.find("gone") != std::string::npos,
          "a sound routing into an undeclared bus refuses naming it");
    CHECK(!Stood("<scenario name=\"t\"><audio><bus id=\"m\"/>"
                 "<sound id=\"s\" uri=\"s.ogg\" positional=\"true\"/></audio></scenario>",
                 bad, error) &&
              error.find("falloffM") != std::string::npos,
          "a positional source without a distance refuses -- a stereo source in a costume");
  }

  Covers("III.13 outshine's mix is a declared bus graph: one master, routes that reach it "
         "or refuse as cycles, sources carrying every bus gain on their route, positional "
         "falloff against one listener, play-by-id answering both ways, and a walk that "
         "takes nothing from the allocator (board:1486)");
  return Report();
}
