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

## What today's build measures (2026-08-25, HEAD 235e3f47, run by the review)

**It builds and it links.** `make` exits 0; `build/outshine-viewer --help` answers with eleven
options. That is real, and it is what unblocked the gate (board:1869).

**It paints nothing.** Both of these, windowed and headless:

```
build/outshine-viewer --headless --frames 3 --into DIR --show .../four-lines.scenario
  BROWSING 1423 case(s) under test
  REFUSED the layout holds no box, so there is nothing to paint and a painting of
  nothing would be indistinguishable from one that failed
  0 stills
```

So `Typeface`, `Pointer`, `Markup`, `Stylesheet`, `Layout` and `Painting` are green in CURRENT
and **STRANDED**: six subsystems and not one pixel from any of them anywhere in the tree
(board:1864). The refusal itself is right -- it is the one that says a blank frame must not pass
for a drawn one -- but the box it wants is the viewer's own face, which the viewer declares.

**And a refusal without a reason.** `apps/viewer/src/main.cpp:150`:

```cpp
if (asked.Scenario.empty()) { Usage(); return 2; }
```

Eleven options, `--cases` and `--case` among them, and asking for the case browser alone exits 2
printing the help with no sentence saying what was wrong. A failure is loud, and loud means it
names itself. `--show` should not be compulsory for a programme whose own face is a scenario.

## What will be true

- [x] `apps/viewer/src/main.cpp` exists and the gate builds it.
- [ ] It DRAWS: a `--headless --frames N --into DIR` run leaves N stills and consecutive ones
      differ. At 235e3f47 it leaves zero and refuses for want of a box.
- [ ] Every refusal carries its reason. `Usage(); return 2` with nothing said is a silent exit,
      and the browser stands without `--show`.
- [ ] `apps/viewer` loads any scenario by path and shows it, with `--show PATH` and nothing
      case-specific in its arguments.
- [ ] Its own chrome is DECLARED -- a markup document the viewer loads, not a string built by
      `+=` in C++ -- and it is declared INSIDE the scenario it shows rather than composited over
      it by code that knows better.
- [ ] `apps/viewer` includes nothing from `test/`: a client does not stand on the test harness.
- [ ] It compiles against `include/` alone, like the driver.
- [ ] Proving test: the viewer shows a corpus case and the driver's drive from the same binary
      with no branch between them.

