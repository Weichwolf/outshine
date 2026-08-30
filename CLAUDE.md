# outshine

A game engine at RAGE/Unreal level, in C++23. The development platform **is** the target: Apple
A18 Pro — 2P+4E cores, 5 GPU cores, 8 GB, Metal 4 — holding **720p60**, measured as p50/p95/p99
over a moving camera and never as a mean.

An engine is an **interactive physics simulation with a focus on graphics**, and each word is a
bound: physically as accurate as NECESSARY, graphically as good as the FRAME BUDGET allows,
temporally DETERMINISTIC. The middle bound is the one "as good as possible" cannot carry — a
target without a ceiling cannot be missed.

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
own shadows the moment the clock moves. **How far the tree is from any of this is `board/`'s to
say, never this page's.**

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
**THE DOOR SPEAKS FILAMENT AND CESIUM; THE MOTOR HOLDS RAGE AND UNREAL.** A client's knowledge
has to transfer, so `include/` uses the names a reader already owns — **Filament** for the renderer
(`Engine` · `Scene` · `View` · `Camera` · `Renderer` · `Material` · `MaterialInstance` ·
`TransformManager`) and **Cesium** for the Earth (`Georeference` · `GlobeAnchor` ·
`LongitudeLatitudeHeight` · `sampleHeight`). Both are Apache 2.0 and both are READABLE, which is
the same admissibility this page already asks of a source. Filament because it is a RENDERER rather
than an engine — the exact layer — and because the people who wrote it ship it on phones, which is
this target's own constraint. Cesium because georeferenced 3D is what it is FOR, and because
Unreal and Unity users already spell a place its way.

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
the way a paper does — inside the item, never as a third column. A source that cannot be read
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

**The third question outranks anything already written down.** A cause recorded in an item or a
commit is a HYPOTHESIS until it is measured again — including one written on this page. They fail
that test often enough that the habit is worth more than any of them: state the measurement before
the work, so being wrong is visible on the day rather than a month later.

## The craft

These are C++ truths rather than decisions about outshine, and they do not move.

- **C++23**, `-Wall -Werror -Wpedantic`, one `-std`. A warning is an error
- **What the compiler can decide is a `static_assert`, never a case** — layout, size, trait,
  catalogue completeness, an enum's exhaustiveness. It is stricter than a suite and cannot be
  skipped, sampled or left unlinked
- **The type system over checkers**: `std::span` / `std::string_view` at boundaries, `std::mdspan`
  for field and instance views, `std::expected` where a refusal carries its reason
- **Private is the DEFAULT** and a wider door justifies itself. A public data member is an
  invariant nobody can hold. Composition usually; inheritance where a stable interface carries
  shared machinery
- **SIMD- and optimisation-friendly**: contiguous, one-width, pointer-free layouts; fast path on
  the hot path; batch over per-item; bounded terms on the frame path — no alloc, lock, disk or
  unbounded block
- **`make` DELETES the comments.** `include/` and `src/client/` keep Doxygen because both are
  DOORS; the rest of `src/` keeps nothing, and seeing that on every build is what forces code that
  speaks for itself. `git log -p` holds every line removed. Prose stands in a PROOF — any source
  carrying `Covers("`
- **A name is a promise.** A word that means something else in Unreal or RAGE spends a reader's
  knowledge against them. The engine's vocabulary is LAW — body, joint, degree of freedom, drive,
  constraint, force, contact, integration — and a car, a wheel, a seat or a door is a SUBJECT a
  scenario assembles. Generators are the exception and the reason is exact: a tree grower's whole
  job is to make one concrete thing
- **WHERE SDL3 SUPPLIES THE STRUCTURE OR THE FUNCTION, IT IS THE ONE USED.** SDL3 is a hard
  dependency rather than a choice, so a second mechanism beside one it already carries is not
  insulation, it is a duplicate that has to be kept true to a driver nobody here wrote. A thin RAII handle
  over an SDL type is ownership and stays; a scheme that re-decides what SDL decides is a finding
- **Every number carries its origin** (derived · measured · `[SET]`) with unit and population. No
  magic numbers; calibration measures, never decides
- **A diagnostic is a declared LABEL**, never a free literal: `namespace Says` at the top of the
  file, `std::format` at the site. The compiler checks the placeholders and a file's ways of
  refusing read as a list
- **A failure is loud.** Accepting a declaration and doing nothing with it is worse than refusing
  it. Something is always drawn; delete on the day you replace; artefacts to the system temp dir,
  never the tree

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
- **FOUR THINGS RUN INDEPENDENTLY — SIM · VIDEO · AUDIO · IO — and what passes between them is a
  SNAPSHOT.** The simulation owns the world and hands the renderer a delta; the renderer draws a
  frame behind and never reaches back; the mixer reads where sources stood when it mixed. **IO is
  the fourth and it is not a task**: a fetch BLOCKS, and a blocking task on a compute worker is a
  worker doing nothing while holding a slot. The two pools are sized by DIFFERENT quantities —
  compute by cores, IO by how many requests may be outstanding — so they cannot be merged, and a
  compute worker is NOTIFIED rather than polling. Unreal separates it (`FIoDispatcher`, the async
  loading thread) and so does RAGE (streaming threads beside `sysTaskManager`); neither lets a
  stall on a disk or a socket cost a core. **Headless is the fast path, not a degraded one** — a
  picture is what makes a run REALTIME. A subsystem that reads another's live state instead of
  its snapshot is the defect, because it puts a wait where a handoff belongs

## Where things live

| | |
|---|---|
| `include/` | the door — **a header is public only if a client cannot use the engine without it** |
| `src/` | the library. **The directory IS the dependency tier**, declared in `test/run.sh` and enforced by `--audit-layers`, which also refuses a cycle |
| `src/generators/` | a library with its own door: a client registers its own beside them, and the tier links with none of the engine behind it |
| `test/` | **the vendor's word and ours stand apart and the directory says which.** `khronos/` · `wpt/` · `test262/` · `geographiclib/` are the corpora; `harness/` their scorers; `outshine/` is ours. `harness/claims/` is a LINTER over the repository and runs from `make lint` |
| `src/client/` | **THE ONE CLIENT**: `build/outshine-client`, the engine through its own door and the camera that measures it. A product is a SCENARIO plus one command, so a second program would be a second door |
| `board/` | one flat directory of work items — see below |
| `Makefile` | the ONE way in: `strip · db · lint · doc · shots · test · suite · clean` |

## What proves what

**Only the vendor corpora prove anything.** Khronos, WPT, test262 and GeographicLib are where a
standards body or a computation carried further than ours states the answer; a case there fails
because the code is wrong. Everything under `test/outshine/` is a REGRESSION NET of unknown grade:
it holds the tree to what the tree already did, which is agreement with ourselves. **That bites
hardest during a refactor** — green means the previous behaviour was preserved, and if that
behaviour was wrong, green is the wrong answer preserved exactly. A red in `outshine/` during a
refactor is INFORMATION, and it is never made green by editing the case.

| grade | it holds | it proves |
|---|---|---|
| **SPEC** | a standards body states the answer | conformance |
| **TRUTH** | a measurement or computation carried further than ours | correctness |
| **SNAPSHOT** | another implementation, frozen | agreement, never correctness |
| **INPUT** | nothing is supplied | that we survive it |

**A BENCHMARK IS A TOOL, NOT A GATE.** A PROOF states an invariant and its negative control goes
RED. A BENCHMARK states a RATE, and a rate has no negative control: it is faster or slower, never
wrong, so it can never earn a tick. It BOUNDS a decision — "this is the number the change has to
beat" — and is quoted in the item that spends it. Every instrument states what it does NOT cover on
the page where it prints, because the mistake it guards against was made here: a subject's rate
quoted about a world.

**`build/outshine-client` IS THE INSTRUMENT AND `test/outshine/places/` SCORES IT.** The camera
lives in `src/client`, whose `reaches` names `base` alone -- so it is held to the same door a
stranger gets. `make shots` stands six real places, streams them, draws them and writes
`build/shots/<place>-<digest>.png` beside what each cost; the six cases run that same command and
apply the oracles to its rows. A number from here always has a picture next to it that can be
looked at, which is what the third question above asks for, and the digest says when a picture
moved without anyone intending it.

Every case is a scenario with an invariant oracle whose truth does not depend on our design. **A
tick is only earned when its proof stands AND its negative control goes red** — a control that
passes proves nothing, and that is the trap that costs most here.

**`make` IS THE ONLY DOOR** and nothing is started by reaching past it:

| | |
|---|---|
| `make` | strip · the library · the generators · the tools beside them |
| `make lint` | clang-format · clang-tidy · the repository's own rules · Doxygen — each held to a baseline that may only SHRINK |
| `make shots` | six places through `build/outshine-client`, pictures to `build/shots` |
| `make test` · `make suite SUITE=x` | the fast gate · one suite |
| `make db` · `make doc` | `compile_commands.json` · the door's documentation |

A strict analysis over 57 000 lines is red on day one and switched off in the first week, so the
count is recorded and a commit may only lower it: new code is held to zero because anything it
adds shows in the total.

## How I work

**Order: repair the VISION first if it is short of the benchmark · rebuild onto it · then close
the feature gaps.** A refactor toward a target short of the benchmark arrives somewhere that still
has to be left.

`board/` is ONE FLAT DIRECTORY. One file = RFC 822 header + markdown body; fields `Type` ·
`State` (open|active) · `Parent` · `Area` · `Tags` · `Depends` · `Supersedes`. Filename
`NNNN_label.md`; **the number is identity, so it is issued once and never again** — the next one
comes from the HISTORY, which remembers every id ever filed, not from the directory, which only
remembers the ones still standing:

    git log --all --diff-filter=A --name-only --format='' | sed -n 's|^board/\([0-9]*\)_.*|\1|p' \
      | sort -n | tail -1

No dates. Titles say what WILL BE TRUE. Commits reference
`board:NNNN`.

**Every item carries the benchmark and the choice** in one line near the top:

    **Benchmark** — Unreal: <what it does>. RAGE: <what it does>. **Taking <which>** because <why>.

An item leaves three ways, and only a CLOSURE passes through `State: active` — recorded in its own
commit BEFORE the work, because that is the only place the board says what has an owner right now.
A WITHDRAWAL says the defect was never there and names what was misread. A REMOVAL says the item
named no step toward the benchmark. Closing is DELETING the file: what it said is in the commit,
and `git log` is the logbook.

**Grep `board/` AND the history before filing.** The directory holds what is open; `git log` holds
what was closed, withdrawn and REMOVED — and filing a removal again overrules that decision by
accident. A duplicate is worked, never written twice; a defect found while working something else
becomes an item in the same round.

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
