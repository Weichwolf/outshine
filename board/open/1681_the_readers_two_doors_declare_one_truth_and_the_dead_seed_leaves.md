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
