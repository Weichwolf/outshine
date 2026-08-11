---
name: outshine
description: How work is done on Outshine — the OSM-based open-world engine (C++17, WebGPU/WASM on Chromium/Edge, worldwide tile server in C). Where things live, how a round runs, who designs and who builds, and the traps this project demonstrably falls into. Load when working on Outshine's code, architecture, scenarios, generators, renderer or world, or when judging whether a change fits.
---

# Outshine

> **A worldwide sandbox at Kingdom Come: Deliverance's picture quality: you walk anywhere, and everything streams, comes into being and is
> placed while you go. The only input is what the tile server delivers.**

**Everything in the repository is English** — code, comments, documents, commit messages.

## What binds

| Place | Content |
|---|---|
| **`CLAUDE.md`** | the rules. Binding, and a violation is wrong even when it works. **Read it first.** |
| `doc/vision.md` | what for, and where the bar sits |
| `doc/architecture.md` | how Outshine is to be built — decisions, not prose |
| `doc/todo.md` | the next steps, in order |

**`doc/` holds three files and gets no fourth.** No spec, no state, no gaps, no journal — a document
describing what the code *does* is the same thing in two languages, and the second one can lie. What was,
is in `git log`. What is, is in the code. **Only correct work is committed**, so there is no second place
where correctness is claimed.

## The tree

```
sim/src/    clients · core · generators · render · units · world
tiles/      fb-tiles, the C tile server
scenarios/  the declared worlds
```

**Core** is the naked world — terrain, classification, atmosphere, clouds, celestial bodies, renderer, and
*where* water is. **Generators** turn that into content — vegetation, structures, infrastructure, the
*appearance* of water — and are exchangeable because they read the same input. A generator is a pure
function of region and ground: it does not draw, knows no camera, no frame, no device. The scheduler knows
where the eye is; the generator never does.

**One program, one entry point.** One object owns world and renderer and is the only thing that builds a
scene; a client is `main()` plus an output medium. Layering is enforced by the build — a target that omits
the renderer is the persistent server, and a breach shows up as a target that stops building.

**wasm32 plus WebGPU is a virtual console.** Fixed heap, declared budgets, and everything else in the tree
is material.

## How a round runs

| Who | What |
|---|---|
| **`engine-architect`** | designs before anything is built, and judges afterwards. Read-only. For an adversarial check, call it **fresh**, without the planning run |
| **`engine-developer`** | builds and measures. Exactly one in the tree — development is strictly serial |

A brief names **goal, constraint and acceptance number**. It names a mechanism only when that mechanism is
established — otherwise it says "find out how X solves this, and propose". **A concrete wrong mechanism in
a brief beats any correct slogan standing next to it.**

## The traps this project falls into

All measured, none invented:

- **Fluency is suspicious.** A plausible sentence about a streamer forms faster than the check that it is
  true. Before building, name the problem in **the vocabulary of the field** — "level load", "streaming",
  "LOD transition", "resection". If no such name presents itself, you do not know the field, and then it
  is research rather than improvisation.
- **Measure before you reach.** Five guesses cost more here than the one measurement anyone should have
  started with. A measurement that refutes your guess is the round's result.
- **The expensive defects are meaning defects, not C++ defects.** An absolute value in a camera-relative
  buffer, 16 of 24 hash bits, a white point taken from the sRGB container, a shoot with a 3 cm minimum
  radius — every one of those lines would pass any review. **Unit and frame of reference are part of a
  number's origin.**
- **Watch the baseline.** A run-wide average is not a zero point when the quantity drifts over the run; an
  event's cost measured against it absorbs the trend and comes out two to three times too large.
- **A comment is a claim without a test.** Six of them lied in a single session. Only the local,
  non-obvious *why*, one line; never what the code does; never a measurement — that decays, the comment
  stays. **A name that needs a comment is the wrong name.**
- **A green gate proves only what it checks.** For ten rounds a passing build proved the browser
  *compiles*, not that it *shows* the same thing. A gate that measures structure would have caught it
  immediately.
- **A confounded finding costs a round.** "No directional light" was a scene at −3.6° sun elevation. Before
  every defect, actively seek the harmless explanation and say why it is ruled out.
- **The reference is Kingdom Come: Deliverance**, and it is demonstrated rather than aspirational — 1080p30 on a PS4, a landscape modelled on real Bohemian regions, and a picture that is above all vegetation and terrain. What it reaches with an art team and a texture budget, we must reach with a function; where a technique of theirs depends on an authored asset, the substitute is named or the gap is stated.
- **The still is the comparison resolution, not the acceptance.** Popping, ghosting, a hitch on stream-in,
  a scatter that ends at a radius — a single frame shows none of them.

## When you get stuck

Do it the way the established ones do. The canon is in `CLAUDE.md`, and the **C++ Core Guidelines are
binding**. Find the source, name it in one line at the decision point, and deviate only with a reason that
stands next to the deviation.

And check the source rather than citing it: Microsoft Flight Simulator does **not** support a claim about
runtime generation — everything there was generated ahead of time in the cloud. For a world that comes
into being while you walk, Guerrilla's *Horizon Zero Dawn* is the evidence.
