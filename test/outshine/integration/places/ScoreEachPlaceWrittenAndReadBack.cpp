#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"
#include "Shell.h"

// READ, WRITE, READ AGAIN, and the two declarations must be the same one.
//
// A format that is only ever read cannot be diffed against what the engine HOLDS, and that
// asymmetry is how a grammar and its reader drift apart. EIGHT drifts were found in one evening,
// each of them a capability no declaration could reach -- `<view>` demanded `follows` and `person`
// and forbade the `<at>` its own reader read; `<render>` read a `<keep>` its grammar refused; an
// audio bus could carry neither reverberation nor a voice; a scene could not name its room.
//
// This case is the counter-control they had none of, and it is RETROSPECTIVE: restoring the old
// `<view>` row puts every place APART with the exact refusal that cost an hour to find by hand.
//
// WHAT IT DOES NOT PROVE. A field the writer never emits round-trips trivially, because both
// writes omit it. It proves the sections the door writes -- world, render, lighting, assets and
// views -- and the whole answer is board:2052: derive both sides from the declaration types, so a
// field cannot be missing from one of them.
int main(void) {
  using namespace outshine::Test;
  std::string said;
  const int closed = Run("build/outshine-client roundtrip", said);
  std::printf("%s", said.c_str());

  size_t held = 0, apart = 0;
  for (const std::string &line : Lines(said)) {
    if (line.rfind("HELD", 0) == 0) { ++held; }
    if (line.rfind("APART", 0) == 0) { ++apart; }
  }
  Note("places written and read back", (double)held, "places");
  Note("places whose two writings differ", (double)apart, "places");

  CHECK(held > 0,
        "the client wrote and read back at least one place, so the emptiness below is a "
        "measurement over a population rather than a statement about an empty one");
  CHECK(apart == 0,
        "**READ, WRITE, READ AGAIN IS THE IDENTITY**: every place this tree renders is declared, "
        "written in the spelling its own reader accepts, read back and written again, and the two "
        "texts are the same one. A section the reader cannot spell shows up here as a refusal, and "
        "a value the engine ignores shows up as one too -- both are declarations an author would "
        "believe and the engine would not act on");
  CHECK(closed == 0, "the client's own verdict agrees with the count above");

  Covers("board:2052 -- the scenario is the public interface written down, and it round-trips");
  return Report();
}
