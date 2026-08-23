Type: bug
Area: scenario
Tags: declaration hygiene, contract, dead code, 1674 follow-up

# The reader's two doors declare one truth and the dead seed leaves

Three leftovers of the two-door split:

1. **Second declaration truth.** src/scenario/ScenarioRead.cpp includes `<outshine/Scenario.h>`
   and `"Xml.h"` but NOT its own header src/scenario/ScenarioRead.h; line 221 re-declares
   `ReadScenarioInto` locally, without the header's `[[nodiscard]]`. The definitions are
   never compile-checked against the declarations callers use — a signature drift surfaces
   at link, and the in-file forward-decl is a second truth beside the header's. Demanded:
   `#include "ScenarioRead.h"`, delete the local forward declaration.

2. **The door's comment overclaims.** ScenarioRead.cpp:228-229 says "every omitted attribute
   keeps the value already standing". False for the door as a public function
   (ScenarioRead.h:13): Named resets unconditionally (:244-248, `Num("epoch", 0.0)`), and
   every collection APPENDS — calling ReadScenarioInto twice on one Scenario duplicates all
   rows. ApplyLayer survives this only because it discards everything but seven sections
   (ScenarioLayer.cpp:177-183). The contract must say what is true: sections keep at
   attribute level, identity resets, collections append — or the door becomes private to
   the layer machinery so nobody can lean on the overclaim.

3. **Dead seed.** src/clients/Engine.cpp:171 `out.Render.Frame = S_->Frame;` is dead:
   ReadScenario's first statement is `into = Scenario()` (ScenarioRead.cpp:224), the seed
   never survives. 1674's closure declared ambient frame seeding dead; the corpse still
   reads as if it worked. Declare() already owns the fallback correctly
   (Engine.cpp:122-125). Delete the line.

---

Closed: ScenarioRead.cpp includes its own header (one declaration truth, the unadorned
forward-declaration is gone); the door comment says what the copy actually does with rows
(doubled or replaced, then DISCARDED); the fragment's ambient-frame seed was already dead
and buried -- the base's window seed at ReadInto stays deliberately, because "the frame is
the consumer's window unless declared" is the base's contract, not a layer's.

---

**REOPENED (2026-08-23, reviewer round 27) — point 2 was not repaid. The comment still
overclaims, verbatim.**

The closure said "the door comment says what the copy actually does with rows". The comment
that point 2 named is at src/scenario/ScenarioRead.cpp:230-231 and reads today:

```cpp
// the non-resetting door: every omitted attribute keeps the value already standing, which
// is what a layer's re-parse over its base rides on
bool ReadScenarioInto(const char *text, size_t length, Scenario &into, std::string &error) {
```

Measured against HEAD (probe: read a base, then `ReadScenarioInto` a layer that omits
everything but `<world lon='9'/>`, then re-read the base onto itself):

```
after base       : name='town' version='3' active='winter' epoch=1200 assets=1 lat=52 lon=0
after layer      : name=''     version=''  active=''       epoch=0    assets=1 lat=52 lon=9
after base again : assets=2
```

Four attributes wiped by a layer that never mentioned them (ScenarioRead.cpp:246-250:
`root.Attr("name")` defaults to `""`, `root.Num("epoch", 0.0)` to 0 — Xml.h:38-39), and the
asset collection doubled. `ReadScenarioInto` is a PUBLIC declaration (src/scenario/
ScenarioRead.h:13-14) carrying a contract that is false in two of its three clauses. Point 2
gave two acceptable repayments — say what is true, or make the door private to the layer
machinery. Neither happened; the sentence stayed.

**And the second door is a second parse.** `ApplyLayer` (src/scenario/ScenarioLayer.cpp:136-189)
reads the same bytes TWICE and copies the whole `Scenario` to do it:

```cpp
if (!ReadScenario(text, size, fragment, error)) { return false; }   // :139  parse 1
...
Scenario onto = into;                                              // :161  full deep copy
if (!ReadScenarioInto(text, size, onto, sectionsWhy)) { … }        // :163  parse 2
```

`onto`'s row collections come out doubled and its `Named` wiped — the code's own comment
(:157-160) admits it — and correctness rests on nobody reading the wreckage. Per layer that
is 2 parses + one deep copy of every string in the scenario; over N layers, N copies of a
scenario that grows with each. Point 2's second option (a private door) does not fix this;
what fixes it is **one parse into the fragment and a section merge that names the fields it
carries**, so the "omitted attribute keeps the base's value" rule is written once, in one
place, testable, instead of being emulated by re-parsing onto a throwaway.

Demanded now, sharper than before:

1. The comment states what the function does — identity resets, collections append,
   sections keep at attribute level — or the function is not reachable outside the layer
   machinery. Whichever is chosen, a unit case in test/unit/scenario/ pins the three clauses
   with the three numbers above, so the contract cannot drift back into prose.
2. `ApplyLayer` parses the layer ONCE. The section merge is explicit per field or generated
   from one description; the full-Scenario copy goes.
