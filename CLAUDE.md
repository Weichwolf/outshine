# outshine

A game engine at RAGE/Unreal level, in C++23. The development platform **is** the target: Apple
A18 Pro — 2P+4E cores, 5 GPU cores, 8 GB, Metal 4 — holding **720p60**, measured as p50/p95/p99
over a moving camera and never as a mean.

An engine is an **interactive physics simulation with a focus on graphics**, and each word is a
bound: physically as accurate as NECESSARY, graphically as good as the FRAME BUDGET allows,
temporally DETERMINISTIC. The middle bound is the one "as good as possible" cannot carry — a
target without a ceiling cannot be missed.

**This page is the AIM and never the state.** Where the tree actually stands, what is open and what
was decided is `board/` and `git log` — read those first, and `make help` for what can be run. A
sentence here that describes today would be a lie within a week, so there is none.

**WHAT THE ENGINE IS LAID OUT FOR.** Textures must WORK — the vendor corpora are the oracle and
their textured path has to be right. But the frame budget is laid out for five things, and they are
the answer to "what actually looks stunning": **high geometry with RECURSIVE generators · many
lights and their shadows · a realistic atmosphere · parameterised materials · reflections and
mirroring**. A texture is somebody else's photograph of a surface; light and composition are the
engine's own and are what a viewer reads. The budget is finite, so every millisecond has a place it
did NOT go, and that list is where it goes.

The world is EARTH and the engine is online by definition: a picture can be made of any place on
it, and what comes out has to be comparable with reality. Elevation always, vector data where a
scenario asks for it, and the sun, the moon, the stars and the weather standing where the place and
the hour put them rather than where a number says. A hand-set sun is a sun that disagrees with its
own shadows the moment the clock moves. **Reachable providers are PRESUMED** -- elevation, vectors,
weather and whatever a scenario declares next are servers this engine may count on, so no design
here pays for their absence, and an offline world is a cache and never a second architecture.
**How far the tree is from any of this is `board/`'s to say, never this page's.**

**WHO AUTHORS: NOBODY, AND THAT IS THE DIFFERENCE.** RAGE and Unreal are engines UNDER an
authoring tool -- a studio places every prop, and the map is content. outshine has no tool for a
person and is not going to get one: the world is DATA (the Earth, fetched), the assets are glTF
an AI built, the game is a SCENARIO an AI declared over a real place, and the engine does the
rest. A Fallout is Boston plus a layer that ruins it; a Cyberpunk is a city plus a layer that
lights it. So everything a level designer does by hand in the references is here either a
GENERATOR or a DECLARATION -- placing, dressing, lighting, populating -- and the door's round trip
(read, write, diff) plus the instrument's picture are the author's only hands and eyes. A verb
that cannot be declared, or a result that cannot be looked at through the door, is a verb the
only author this engine has cannot use. **A WORLDWIDE SANDBOX, data-driven and never authored.**

## Why

Because a frame that holds 16.7 ms while a world lives inside it is one of the few things in
software that is unmistakably *done well* — and you can see it. RAGE and Unreal got there. Not by
being clever in one place, but by being right in a thousand, and by refusing to leave a decision
half-made because it was expensive to finish.

I want that. Not a port of it, not a tribute to it: the same standard, reached here, on hardware
that fits in a pocket. A road you can drive down with trees that grew where they stand, a shadow
that falls where the sun says it should, an engine note that comes from the machine making it
rather than from a file somebody recorded. And underneath it, code a stranger can read and say
"yes, that is how it should be done".

Every rule below exists to protect that. None of them is bureaucracy — each one is a day already
paid for, written down so it is not paid twice. When a rule stops serving the engine, it goes.

## What done means

**outshine is to be a REFERENCE DESIGN — something a technical university could teach from.** That
is a harder bar than "correct" and it changes what finishing means: a structural answer has to be
LEGIBLE, defensible from first principles, and carry its reason where the decision is made. Code
that works and cannot be explained is unfinished; a number without its derivation is unfinished;
an abstraction that needs the author present to be understood is unfinished. The reader I am
writing for is a competent stranger, not myself in a week.

Not "it works". The bar is an answer RAGE or Unreal would recognise as their own, or a better one
I can defend with a measurement of THIS tree.

## How I decide

For every structural question, one of RAGE or Unreal already has the right answer. My job is to
hold the better of the two, never to invent a third. Where they agree the matter is closed. Where
they differ, the decision is written down with its reason — the reason is the part I owe. Where
neither faces the question, the item says exactly that and says why the choice is mine.

**A measurement of THIS tree outranks both.**

**THE HARDWARE HAS A VOICE TOO, AND IT IS MEASURED, NEVER ASSUMED.** The target is one GPU with
properties the references were not designed around -- tile memory a deferred pass never leaves,
mesh shaders that tessellate a height field without a vertex upload, ray-tracing units. Where
the target's own answer differs from both references (Apple's tile shading against Filament's
froxels), the item builds the reference's path, measures the hardware's beside it on THIS tree,
and keeps the one the number picks. A hardware feature named without a measurement is a
brochure, not a decision.

**THE DOOR SPEAKS FILAMENT AND CESIUM; THE MOTOR HOLDS RAGE AND UNREAL.** `include/` uses the names
a reader already owns — **Filament** for the renderer (`Engine` · `Scene` · `View` · `Camera` · `Renderer` · `Material` ·
`MaterialInstance` · `TransformManager`) and **Cesium** for the Earth (`Georeference` ·
`GlobeAnchor` · `LongitudeLatitudeHeight` · `sampleHeight`). Both are Apache 2.0 and READABLE,
which is the admissibility this page asks of any source. Filament because it is a RENDERER rather
than an engine and its authors ship it on phones, which is this target's constraint; Cesium because
georeferenced 3D is what it is FOR.

**Not Godot**, and the reason is a rule rather than a preference: its vocabulary is a NODE TREE —
`Node3D`, `add_child`, `owner` — and a scenario over a store has no such hierarchy. A name is a
promise, and borrowing those names would promise a shape this engine does not have.

Behind the door the answer is still the better of RAGE and Unreal. **Outward the names a client
expects; inward the engineering those two paid for.** Where a door name and a motor name disagree,
the door keeps the client's word and the motor keeps its own — and the translation happens once, at
the boundary, rather than in the reader's head.

**Two bodies decide, and a third may be CITED.** The table has exactly two columns because a
column has to be READABLE: Unreal is source, RAGE is reconstruction. Ubisoft's three engines —
Anvil, Snowdrop, Dunia — are strong where outshine is weak (crowds, vegetation, procedural
authorship) and none of them can be read, so a column for them would be a vote cast from a slide
deck. **id Tech 4 is GPL and therefore readable**, so it may stand beside a decision as evidence
the way a paper does — inside the item, never as a third column. **Jolt Physics is MIT and
readable** and is the one body that promises determinism across platforms, so for a physics
question it is cited before Chaos. **Cesium** is cited for anything that streams a planet. A source that cannot be read
supports a TECHNIQUE and never decides a STRUCTURE.

## Before I write

Three questions, answered IN WRITING before a type, a function or a file exists. They land in the
commit, which is why they cannot be skipped quietly — an empty answer is visible.

1. **What does Unreal do here, and what does RAGE do?** If I cannot name it, I do not understand
   the problem yet
2. **Does this already exist here, unreachable?** `grep` first. A complete capability no
   declaration reaches is the commonest defect in this tree, and writing a second one is the worst
   outcome available: now there are two, and neither is right
3. **If it draws, LOOK AT IT before believing any number.** A count needs a hypothesis to mean
   anything and an image needs none: five cases once passed `more than one colour` on a sky
   gradient over an empty world. `make shots` keeps every picture under its own DIGEST, so a frame
   that moved says so — and the expectation is written down before the looking
4. **What measurement will show I was wrong?** Name the case, the audit flag or the number, and
   what it reads if the change is bad. A change with no such number is a guess wearing a commit
   message

**A MOVED DIGEST IS ACCEPTED WITH THREE THINGS OR NOT AT ALL**: `test/scripts/pixels.py`'s count
against the kept reference, WHERE the pixels are and why they moved (a silhouette, a cluster, a
seat), and the picture looked at. Measured 2026-09-04: a 618-pixel move at CentralPark that read
as "small" was a tower culled by a wrong occlusion window; the coordinates named it, the count
did not. The references under `build/shots/reference/` are the regression test of this tree, and
a change that cannot show all three goes back.

**A PIXEL COUNT IS NOT A LOOK.** Measured 2026-09-04: a rewrite of one function's signature moved
59% of OldTown's pixels, and 59% reads as "a lot changed" -- large, arguable, possibly acceptable.
The IMAGE said the entire city was gone, grass to the horizon, in one second. The number and the
picture answered different questions, and only one of them was the question.

**The third question outranks anything already written down.** A cause recorded in an item or a
commit is a HYPOTHESIS until it is measured again — including one written on this page. They fail
that test often enough that the habit is worth more than any of them: state the measurement before
the work, so being wrong is visible on the day rather than a month later.

## The craft

These are C++ truths rather than decisions about outshine, and they do not move.
- **C++23**, `-Wall -Werror -Wpedantic`, one `-std`; a warning is an error
- **What the compiler can decide is a `static_assert`, never a case** — layout, size, trait,
  catalogue completeness, an enum's exhaustiveness. Stricter than a suite, and unskippable
- **`constexpr` WHEREVER IT IS POSSIBLE, and the same for the values it feeds.** A number the
  compiler can compute belongs in the binary rather than in the frame, and a function that is
  `constexpr` is a function a `static_assert` can INTERROGATE — so this rule and the one above it
  are one rule: constexpr is what makes the assert possible, and the assert is what makes the
  constexpr worth writing. `consteval` where the answer must not reach run time at all. A table
  built at startup is a table that could have been built at compile time, and a derivation left to
  run time is a derivation nothing checks. **clang-tidy does the rest** — every rule that a checker
  already enforces stays out of this page
- **The type system over checkers**: `std::span` / `std::string_view` at boundaries, `std::mdspan`
  for field and instance views, `std::expected` where a refusal carries its reason
- **`alignas` BELONGS AT THE DEVICE BOUNDARY, and equality is `operator==`, never `memcmp`.** A
  record a driver reads keeps the alignment that driver's rows demand and a `static_assert` on its
  size, because the layout is the boundary's word and not ours -- the same rule as SDL3's. But that
  alignment PADS, and `memcmp` compares the bytes nobody wrote: padding, and `-0.0` against `0.0`.
  A defaulted `operator==` compares the members and cannot see either. Measured here: twelve
  indeterminate bytes decided whether a sky table was recomputed, and the stage next door had
  already patched the same defect by hand with a `Pad` member -- a workaround is evidence that the
  rule was missing
- **Private is the DEFAULT** and a wider door justifies itself; a public data member is an
  invariant nobody can hold. Composition usually; inheritance where a stable interface carries
  shared machinery
- **THE COST OF A LINE IS NOT IN THE LINE.** An array index and a hash lookup read the same and
  differ by two orders of magnitude, because one of them MISSES — and a miss is ~100 ns, ~300
  cycles, long enough to have scanned a kilobyte of contiguous memory instead. So the LAYOUT sets
  the speed and the algorithm only sets the shape: contiguous, one-width, pointer-free, the fields
  read together stored together. A `vector<vector<T>>` or a map on a hot path is a pointer chase
  per element wearing a container's name — measured here, a junction solver walked an
  `unordered_map` 24 times and became one flat node-sorted vector with the same arithmetic to the
  bit. Batch over per-item, fast path on the hot path, and nothing on the frame path allocates,
  locks, touches disk or blocks unbounded. **Ask what the MACHINE does, not what the line says**:
  which loads, how far apart, how many times
- **ON THE FRAME PATH AN ENTITY COSTS O(1) AND THE FRAME COSTS O(N); a preload may pay O(N log N)
  and never O(N²).** But the CLASS is the weaker half of that rule and the constant is the number
  of cache lines touched -- measured here on one problem: a hash map is O(N) and took 565 ms, a
  comparison sort is O(N log N) and took 425 ms, a counting sort is O(N) and took 92 ms. The class
  predicted the wrong order; the memory accesses predicted the right one. Where a bound genuinely
  exceeds O(N) the answer is a STRUCTURE, not a faster loop: many lights times many objects is
  clustered, not iterated
- **`make` DELETES the comments.** `include/` and `src/client/` keep Doxygen because both are
  DOORS; the rest of `src/` keeps nothing, and seeing that on every build is what forces code that
  speaks for itself. `git log -p` holds every line removed. Prose stands in a PROOF — any source
  carrying `Covers("`
- **ONE SOURCE PER RULE, AND EVERYTHING ELSE DERIVES FROM IT VISIBLY.** This is SQL's normal form
  carried into code: a functional dependency lives in one place, or the copies drift. But the test
  is **do these change together**, never do these look alike -- and both mistakes were made here in
  one day. `[12 + axis]` stood eight times across five files and a regex would have unified all of
  them; it was FOUR meanings, six reading a translation column and two transforming a point, and
  they never change together. The sun's direction stood twice, once negated, so it did not look
  alike at all and no duplicate finder would have seen it -- and those two cannot change apart
  without lighting a scene from one side and shadowing it from the other. Duplication is cheaper
  than the wrong abstraction: two things forced into one source grow a flag, then a second flag,
  then a `bool isTheOtherCase`. **For NUMBERS the rule is stricter and has no exception**: a value
  that follows from others is derived and never restated. `kSPerHour = kSPerMin * kMinPerHour`, not
  `3600` -- the second is not duplication, it is an unstated derivation, which is worse because
  nothing checks it. `VisualRangeM(haze)` is the same idea as a function: the rule is the source
  and the number falls out of it
- **A NAME IS THE ONE THE READER EXPECTS, and the reader is the engineer arriving from Unreal,
  Filament or a textbook -- never this tree's own metaphor.** A verb is `Set`, `Get`, `Place`,
  `Remove`, `Build`, `Update`, `Attach`; a class is the noun of what it holds. `Hands`, `Wears`,
  `Framed`, `Forgets`, `Restand`, `Grounds` cost a reader a grep per call site, and measured here
  in one day: a session spent more reads on names than on design. A name that has to be looked
  up is a name that is wrong, and the sweep that repairs the old ones is board:2139
- **A name is a promise.** A word that means something else in Unreal or RAGE spends a reader's
  knowledge against them. The engine's vocabulary is LAW — body, joint, degree of freedom, drive,
  constraint, force, contact, integration — and a car, a seat or a door is a SUBJECT a scenario
  assembles. Generators are the exception: a tree grower's whole job is to make one concrete thing
- **WHERE SDL3 SUPPLIES THE STRUCTURE OR THE FUNCTION, IT IS THE ONE USED.** SDL3 is a hard
  dependency rather than a choice, so a second mechanism beside one it already carries is not
  insulation, it is a duplicate that has to be kept true to a driver nobody here wrote. A thin RAII handle
  over an SDL type is ownership and stays; a scheme that re-decides what SDL decides is a finding
- **TELEMETRY IS A TOOL FOR PEOPLE, AND I AM NOT ONE.** A person plants a counter ON SPEC because
  a rebuild costs minutes and the moment may not come again; I rebuild this tree in twenty seconds
  and `make shots` hands me the same place back. So a number worth keeping is one a CLIENT reads —
  frame time, memory, triangles, whether the preload finished — and everything else is built the
  day it is needed and removed the same day. A count that states a CORRECTNESS claim is not
  telemetry at all: `houses buried in the ground` belongs in a case with an oracle that goes RED,
  never in a line somebody might read. Measured: fifteen counters in the building mesher, threaded
  through ten functions, printed by a block that never executed — and the round that made them
  tidier instead of asking whether they could go ADDED four findings
- **Every number carries its origin** (derived · measured · `[SET]`) with unit and population;
  calibration measures, never decides
- **A diagnostic is a declared LABEL**, never a free literal: `namespace Says` at the top of the
  file, `std::format` at the site. The compiler checks the placeholders and a file's ways of
  refusing read as a list
- **A failure is loud.** Accepting a declaration and doing nothing with it is worse than refusing
  it. Delete on the day you replace; artefacts to the system temp dir or `build/`, never the tree
  -- `compile_commands.json` is the one exception, because clangd looks for it at the root, and it
  is gitignored

## The invariants

Four architectural commitments. Everything else is a decision an item can revisit; these are not.

- **Precision has ONE boundary and it is the camera.** Scene keeps 64-bit positions; the renderer
  is camera-relative in 32-bit. A `float` holding a world position is a defect; a `double`
  reaching a shader is a different one
- **ONE WORLD; everything else is a VIEW.** One space is a convention, one HOLDER is the thing —
  the second holder is what makes two subsystems disagree about the same place. The frame picks
  ONE pre-view translation and every view, light and instance transform builds against that one
- **DECLARED, NOT CODED.** Scenarios declare, the engine behaves. Content = data, engine = verbs.
  A section NOT declared decides nothing — the engine's own default stands in its place, never the
  zeroes of a struct nobody filled in. A scenario is a STREAM: `Declare` seeds, then parts enter
  and leave, and the work a declaration causes is proportional to what CHANGED
- **DETERMINISM IS COMPULSORY OUTSIDE THE SHADERS.** The same declaration renders the same bytes,
  twice, on this machine — so anything assembled from work that ran on more than one thread is
  combined in a DECLARED order and never in completion order. Both references depend on it: RAGE's
  replay plays a drive back frame for frame, and Unreal's automation compares screenshots bit for
  bit and calls a wandering one a streaming bug. `make shots` writes every picture under its own
  digest, which is how a lost determinism is noticed the same day.
- **PROVIDERS ANSWER · GENERATORS EXPAND · THE ENGINE HOLDS THE VERBS · THE RENDERER GIVES THE
  LOOK.** Four roles, and the line between them is TESTABLE rather than a matter of taste. A
  PROVIDER answers a question that has one right answer — how high is it here, what does OSM say
  stands there, where is the sun at 17:40 — and it answers the same way whether it reads SRTM,
  MOLA or noise, because the role is the ANSWER and never the source. A GENERATOR EXPANDS: a
  loader is linear, N bytes in and N bytes out, while eight outline points become four hundred
  triangles and a seed becomes twenty thousand. So the question that sorts them is "can this be
  checked against a truth OUTSIDE this tree" — the height of a point can (SRTM), a house outline
  can (OSM), that house's untagged HEIGHT cannot, and neither can a cloud or the shape of a tree.
  The ENGINE owns the verbs — fetch, hold, ask, place, draw, simulate, hold the frame — and the
  RENDERER owns the LOOK, which is why the atmosphere belongs to the engine and only a cloud's
  FORM to a generator: scattering is physics, form is invention. A generator therefore hands over
  GEOMETRY and a material, never a light. **The planet is a PARAMETER and the Earth is the
  YARDSTICK**: a Mars needs no other engine, only other providers and another catalogue — and it
  is the Earth, alone, that a photograph can argue with
- **FOUR THINGS RUN INDEPENDENTLY — SIM · VIDEO · AUDIO · IO — and what passes between them is a
  SNAPSHOT.** The simulation owns the world and hands the renderer a delta; the renderer draws a
  frame behind and never reaches back; the mixer reads where sources stood when it mixed. **IO is
  the fourth and it is not a task**: a fetch BLOCKS, and a blocking task on a compute worker is a
  worker doing nothing while holding a slot. The two pools are sized by DIFFERENT quantities —
  compute by cores, IO by how many requests may be outstanding — so they cannot be merged, and a
  compute worker is NOTIFIED rather than polling. Unreal separates it (`FIoDispatcher`) and so does
  RAGE (streaming threads beside `sysTaskManager`). **Headless is the fast path, not a degraded
  one.** A subsystem that reads another's live state instead of its snapshot is the defect, because
  it puts a wait where a handoff belongs

## How the tree is arranged

Principles and not a map, because a map goes stale the day a directory moves.

- **A header is PUBLIC only if a client cannot use the engine without it.** The public headers are
  the door and nothing else stands in it
- **A directory IS a dependency tier** and each carries a `reaches` file naming what it may see.
  The include path is DERIVED from that one declaration, so a cross-tier include fails at the
  `#include` with a file and a line instead of being reported afterwards — what Unreal spends
  `Build.cs` on
- **The generators are a library with their own door**: a client registers its own beside them, and
  that tier links with none of the engine behind it
- **THERE IS ONE CLIENT**, the engine through its own door and the camera that measures it: a
  product is a SCENARIO plus one command, so a second program would be a second door
- **The vendor's word and ours stand apart, and the directory says which.** `board/` is one flat
  directory of work items, and `make` is the only way in

## What proves what

**Only a VENDOR CORPUS proves anything** — where a standards body, or a computation carried further
than ours, states the answer. Everything we wrote ourselves is a REGRESSION NET of unknown grade: it
holds the tree to what the tree already did, which is agreement with ourselves. **That bites
hardest during a refactor** — green means the previous behaviour was preserved, and if that
behaviour was wrong, green is the wrong answer preserved exactly. A red there is INFORMATION and is
never made green by editing the case.

**Two exceptions, both narrow.** A check that pins a SPELLING rather than a property is
mis-specified and the CHECK changes. And a declared CEILING is a baseline: it may only FALL, and
lowering it after a repair is the discipline. Everything else red stays red.

| grade | it holds | it proves |
|---|---|---|
| **SPEC** | a standards body states the answer | conformance |
| **TRUTH** | a measurement or computation carried further than ours | correctness |
| **SNAPSHOT** | another implementation, frozen | agreement, never correctness |
| **INPUT** | nothing is supplied | that we survive it |

**A BENCHMARK IS A TOOL, NOT A GATE.** A PROOF states an invariant and its negative control goes
RED. A RATE has no negative control — it is faster or slower, never wrong — so it can never earn a
tick. It BOUNDS a decision, "the number the change has to beat", and is quoted in the item that
spends it. Every instrument states what it does NOT cover where it prints, because the mistake it
guards against was made here: a subject's rate quoted about a world.

**THE CLIENT IS THE INSTRUMENT AND THE CASES SCORE IT.** `make shots` stands real places on Earth
and writes each picture under its own DIGEST beside what it cost; the cases run that same command
and apply their oracles to its rows. A number from here always has a picture beside it, and the
digest says when one moved unintended. The instrument reaches the door and nothing else, so it is
held to what a stranger gets.

Every case is a scenario with an invariant oracle whose truth does not depend on our design. **A
tick is earned only when its proof stands AND its negative control goes red** — a control that
passes proves nothing, the trap that costs most here.

**`make` IS THE ONLY DOOR** and nothing is started by reaching past it:

| | |
|---|---|
| `make` | strip the comments, build the library and the generators, and the client beside them |
| `make lint` | format · static analysis · the repository's own rules · the door's documentation |
| `make shots` | the places, through the client, each picture under its digest |
| `make test` · `make suite` | the fast gate · one suite by name |
| `make db` · `make doc` | the compile database · the generated documentation |

**Every baseline may only SHRINK.** A strict analysis over a grown tree is red on day one and
switched off in the first week; a recorded count that a commit may lower and never raise holds new
code to zero and lets old code be repaired at the pace it is touched. `make help` is the list.

## How I work

**Order: repair the VISION first if it is short of the benchmark · rebuild onto it · then close
the feature gaps.** A refactor toward a short target arrives somewhere that still has to be left.

**A BATCH IS AS BIG AS THE MEASUREMENT CAN STILL ATTRIBUTE**, and the bound is exactly that: when
a digest moves, the batch has to be small enough to name which change moved it. The digest is the
SAFETY, never the metronome — repairs in different files batch freely, two changes to one behaviour
do not.

**A FUNCTION A LATER GOAL WILL OPEN IS OPENED ONCE.** The order above is a priority, not a wall:
cutting a function now, on an earlier goal's terms, when a later goal is about to reopen it is the
same defect as refactoring toward a short target — applied to my own plan instead of to the tree.

**THE GATES RUN ALONE, AND THE NUMBER COMES FROM THE RUN.** `make`, `make test`, `make shots` and
`make lint` share one nest and one tree. Editing while one runs makes it compile a half-written
file and report BUILD on cases that are fine; starting a second makes
`TheNestRefusesASecondRunner` correctly go red about ME. No edit and no second build until it has
printed its trailer. And a gate cut short by a timeout leaves its report STALE: a count read out of
`build/lint/tidy.unique` after an interrupted run is the PREVIOUS run's, which is how a commit here
once claimed a number that had already risen.

**THE COMPILER IS THE CHEAPEST ORACLE IN THE TREE, and a suite is the most expensive.**
`c++ -std=c++23 -fsyntax-only -Iinclude <file>` answers "did I catch every call site" in a second;
a suite answers it in three minutes and one site at a time. Measured: four broken calls in one
file, found by four separate suite runs, nine minutes, where one syntax check would have listed
all four. Let the compiler judge SHAPE and keep the suite for BEHAVIOUR.

**A SWEEP OVER A WORD IS A SWEEP OVER FOUR MEANINGS.** `[12 + axis]` appeared eight times in five
files; six read a translation column and two transformed a point. `grep -c` says eight copies and
is wrong. Read every site before writing the regex, and prefer a rename the compiler can refuse:
change the declaration first, then let the errors name the callers.

**Every item carries the benchmark and the choice** — what Unreal does, what RAGE does, which is
taken and why. An item that cannot say it is not understood yet, and writing that line is most of
the thinking. **Titles say what WILL BE TRUE**: one in the present tense is a complaint, one in the
future is a target somebody can aim at.

**THREE CONVENTIONS ARE WRITTEN DOWN BECAUSE BREAKING THEM IS SILENT AND IRREVERSIBLE.** Everything
else about the board is legible from the board itself and is not this page's business.

- **An item's number is issued ONCE and never again**, and the next one comes from the HISTORY,
  which remembers every id ever filed — not from the directory, which remembers only what is still
  open. Taking it from the directory reuses the number of something closed, and two things then
  share an identity for good
- **Closing an item is DELETING the file.** What it said is in the commit and `git log` is the
  logbook, so the directory holds only what is OPEN and can be read at a glance. A `State: closed`
  left behind makes the directory stop meaning what it claims
- **`active` is said in the item's own commit BEFORE the work** -- the board's only owner mark.
  Several may stand on one chain, each naming what it waits on

Grep the history before filing: a removal was a decision, and filing it again overrules that
decision by accident.

**A defect found while working something else becomes an item in the same round**, even if it
closes in that round: the alternative is a defect only one person ever knew about.

## What goes wrong

Measured failure modes, each of which cost a day here.

| trap | what it looks like | the guard |
|---|---|---|
| **a gate blind to a path** | vendor cases green while engine cases are red, because the harness bypasses the engine's own submission | know which path each case exercises; name what the gate does not cover |
| **a blind rename** | one regex over a word four unrelated types share | rename per type, and let the compiler be the oracle |
| **an inverted premise** | "this tree has no joints" — it had one, misnamed | measure the thing before filing the item about it |
| **a measure that cannot see** | a count that missed every source without a header — or one that counted ITSELF, because the walk looked in the file it was measuring | ask what the measure cannot see before trusting the number it produced |
| **a truncated count** | a `head -24` inside the pipeline that produced a declared ceiling | a declared number is quoted rather than re-derived, so it has to be right before it is written |
| **a green negative control** | the control passes, so the proof proves nothing | restate the claim or delete it; never keep a false proof |
| **a watcher that waits on itself** | `until ! pgrep -f "test/run.sh"` never returns, because pgrep matches its OWN command line -- 33 minutes spent waiting for a process to end that was the wait | ask what a check must NOT see, which is the same question as asking what it cannot see |
| **a finding that is not the defect** | the checker says "declared twice" and BOTH definitions are dead; or the two are different quantities sharing one word | read both sites before repairing either. Three of three went this way in one session, and repairing the reported thing would have fixed none of them |
| **a rename that moves the collision** | `Surface` renamed to `Meshed`, which another header already owned: the count fell by five instead of six and the claim caught it | a rename is only a rename if the NEW name is checked as carefully as the old one was |
| **a case green on a stale binary** | the program proving the generators link alone was written against an API that no longer existed, at four call sites, and passed -- until a header change forced the rebuild | a gate whose freshness check cannot see headers is guarding yesterday's API |
