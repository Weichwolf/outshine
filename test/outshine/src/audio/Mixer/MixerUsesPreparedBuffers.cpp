#include "Mixer.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <new>
#include <span>
#include <string>

namespace {
size_t allocations = 0;
bool tracking = false;

void *Allocate(size_t bytes) {
  if (tracking) { ++allocations; }
  if (void *memory = std::malloc(bytes == 0 ? 1 : bytes)) { return memory; }
  std::abort();
}
}

void *operator new(size_t bytes) {
  return Allocate(bytes);
}

void *operator new[](size_t bytes) {
  return Allocate(bytes);
}

void operator delete(void *memory) noexcept {
  std::free(memory);
}

void operator delete[](void *memory) noexcept {
  std::free(memory);
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  tracking = true;
  void *probe = ::operator new(64);
  ::operator delete(probe);
  tracking = false;
  CHECK(allocations == 1, "allocation observer sees direct C++ allocation");
  std::array<Scenario::Bus, 1> buses{};
  buses[0].Id = "master";
  buses[0].Reverberates.Declared = true;
  buses[0].Reverberates.SecondsRt60 = 0.4;
  buses[0].Reverberates.WetShare = 0.2;
  std::array<Scenario::Sound, 2> sounds{};
  sounds[0].Id = "tone";
  sounds[0].SendShare = 0.3;
  Scenario::Voice oscillator;
  oscillator.Id = "osc";
  oscillator.Parameters = {{"frequency", "1000"}};
  Scenario::Voice filter;
  filter.Id = "filter";
  filter.Does = Scenario::Makes::Biquad;
  filter.From = {"osc"};
  Scenario::Voice delay;
  delay.Id = "delay";
  delay.Does = Scenario::Makes::Delay;
  delay.From = {"filter"};
  delay.Parameters = {{"delayS", "0.003"}, {"feedback", "0.3"}};
  Scenario::Voice output;
  output.Id = "output";
  output.Does = Scenario::Makes::Mix;
  output.From = {"osc", "delay"};
  sounds[0].Graph = {oscillator, filter, delay, output};
  sounds[1].Id = "quiet";
  sounds[1].GainDb = -40;
  sounds[1].Graph = {oscillator};
  sounds[1].Heard.Positional = true;
  sounds[1].Heard.BlockedHz = 800;
  Audio::Mixer whole;
  Audio::Mixer split;
  CHECK(whole.Stands(buses, sounds, 48000) && split.Stands(buses, sounds, 48000),
        "stateful mixers prepared");
  std::array<Audio::Heard, 2> sources{};
  sources[0].Id = "tone";
  sources[0].Standing = true;
  sources[1].Id = "quiet";
  sources[1].Standing = true;
  sources[1].AtM[0] = 2;
  sources[1].Blocked = 0.5;
  std::array<float, 8194> continuous{};
  std::array<float, 8194> partitioned{};
  std::string error;
  allocations = 0;
  tracking = true;
  const bool rendered = whole.Fills(continuous, sources, {}, error);
  tracking = false;
  CHECK(rendered && allocations == 0, "first large block uses no C++ heap allocation");
  constexpr std::array<size_t, 5> sizes{1, 127, 256, 257, 1025};
  size_t offset = 0;
  size_t next = 0;
  while (offset < partitioned.size()) {
    const size_t count = std::min(sizes[next++ % sizes.size()] * 2, partitioned.size() - offset);
    allocations = 0;
    tracking = true;
    const bool filled =
        split.Fills(std::span(partitioned).subspan(offset, count), sources, {}, error);
    tracking = false;
    CHECK(filled && allocations == 0, "changing callback size needs no C++ heap allocation");
    offset += count;
  }
  CHECK(continuous == partitioned, "filter, delay and hall are independent of callback partition");
  CHECK(continuous[2] > 0, "the source is audible rather than an empty fast path");
  return Report();
}
