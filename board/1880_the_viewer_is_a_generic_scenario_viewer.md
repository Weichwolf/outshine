Type: feature
State: open
Area: apps
Tags: viewer, scenario

# The viewer shows any scenario, and adds nothing to it but its own face

`tools/viewer` moved to `apps/viewer`: it is not development support, it is a PRODUCT — the
second client outshine has, and the one that proves the door twice over.

**What it becomes when every case is a scenario: THE VIEWER IS ITSELF A SCENARIO, merged with
the case it shows.** It builds its face — the corpus list, the case list, the status line — as a
declared markup/script document from what it found, and that document is a LAYER applied over
the case's own scenario. The merge is the mechanism the tree already has (`ApplyLayer`,
board:1490: a mod is a layer the scenario orders), not a new one.

That is why it contributes nothing but its own face: there is no compositing step, no wrapper,
no second render path. One scenario goes in — the case, with the viewer layered over it — and
the engine runs it like any other.

That makes it the sharpest test of board:1879 there is: if the viewer can show a Khronos case, a
WPT case and the driver's own drive without branching on which is which, then a case really is a
scenario and the door really is two headers.

## What today's build measures

Two defects surfaced the moment the viewer was made a PROGRAMME rather than a case:

| | |
|---|---|
| `apps/viewer/src/parts/Chrome.cpp:142` onward | it builds its own UI by CONCATENATING HTML STRINGS in C++ -- `out += "</style><body><div class=frame>"`. Content is data and the engine is verbs; a face assembled by string arithmetic is a face nobody can redeclare, and it is the same defect as an embedded shader |
| `apps/viewer/src/main.cpp:18` | it includes `RenderCase.h` from `test/harness/shared` -- a programme under `apps/` standing on the TEST harness. That is corpus vocabulary compiled into a client, which is exactly what a generic viewer may not have |

Neither is a build accident: they are what "generic scenario viewer" costs. The viewer knows
which corpus a case came from, and it writes its face in C++.

## What will be true

- [ ] `apps/viewer` loads any scenario by path and shows it, with `--scenario PATH` and nothing
      case-specific in its arguments.
- [ ] Its own chrome is DECLARED -- a markup document the viewer loads, not a string built by
      `+=` in C++ -- and it is declared INSIDE the scenario it shows rather than composited over
      it by code that knows better.
- [ ] `apps/viewer` includes nothing from `test/`: a client does not stand on the test harness.
- [ ] It compiles against `include/` alone, like the driver.
- [ ] Proving test: the viewer shows a corpus case and the driver's drive from the same binary
      with no branch between them.

## Comments

- 2026-08-25 — filed on the owner's instruction when `tools/` was deleted. `tools/host` went with
  it: `DelayedTransport` had no caller left after the unit cases were removed.
