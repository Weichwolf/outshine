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

## What today's build measures (2026-08-25, HEAD d4c8784c, run by the review)

**It builds, it links and IT PAINTS.**

```
build/outshine-viewer --headless --show apps/driver/src/f31.scenario \
  --assets .../apps-driver-f31 --frames 3 --into DIR
  BROWSING 1423 case(s) under test
  SHOWING apps/driver/src/f31.scenario at 1280x720, headless
  SHOWED 3 frame(s)
```

`frame003.png` carries the corpus column ("ALL (1423)", "khronos (448)",
"test262 (813)", "wpt (162)"), the case list, the selected row in its highlight
and the status line "1423 CASES" — three faces, two sizes, no character standing in for
another. `Typeface`, `Pointer`, `Markup`, `Stylesheet`, `Layout` and `Painting` LEAVE STRANDED
in CURRENT this round, which is the single largest movement on the distance axis since it was
first measured (board:1864). What the text path still does inside the draw is board:1892.

Last round's measurement — *it paints nothing*, `--show four-lines.scenario` refusing for want
of a box — was taken on a scenario that declares no surfaces. The refusal was right and the
conclusion drawn from it was wrong.

**And a refusal without a reason.** `apps/viewer/src/main.cpp:150`:

```cpp
if (asked.Scenario.empty()) { Usage(); return 2; }
```

Eleven options, `--cases` and `--case` among them, and asking for the case browser alone exits 2
printing the help with no sentence saying what was wrong. A failure is loud, and loud means it
names itself. `--show` should not be compulsory for a programme whose own face is a scenario.

## What will be true

- [x] `apps/viewer/src/main.cpp` exists and the gate builds it.
- [x] It DRAWS: a `--headless --frames N --into DIR` run leaves N stills. At d4c8784c three
      frames of the driver's scenario carry the browser's face and the F31 beside it.
- [ ] Consecutive stills DIFFER when the thing shown moves — not yet checked with a moving case.
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

