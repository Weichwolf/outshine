Type: issue
State: open
Area: test, scenario
Tags: corpora, door

# A corpus case is a scenario the engine loads and runs

**The front door is: outshine loads a scenario and runs it.** Nothing else. Not a script verb,
not a layout verb, not a parser class — a client declares a scenario and the engine behaves.

Everything under `test/` may use `include/outshine/` and NOTHING of `src/`. Today the three
corpus scorers break that rule, and they break it because the door has no way to ask what they
ask:

| scorer | what it reaches into | what it actually wants |
|---|---|---|
| `harness/test262/js` | `Script.h`, `program.Run(host, error)` | run this ECMAScript and tell me whether it threw |
| `harness/wpt/css` | `Markup.h`, `Style.h`, `Layout.h` | lay out this document with this stylesheet and give me the boxes |
| `harness/khronos/glTF` | half the render tree via `RenderCase.h` | stand this glTF up and render it at this camera |

`test/harness/shared/` is likewise only for TEST-specific work — manifest reading, prune
bookkeeping, EXR comparison. It may not carry engine internals to get there.

## What will be true

- [ ] A corpus case is expressed as a SCENARIO: the manifest becomes a declaration the engine
      reads, and what the case asserts is what the engine did with it — a picture, a refusal, a
      measured box.
- [ ] `test/` compiles with `-Iinclude` and `-Itest/harness/shared` alone; a claim walks every
      source under `test/` and finds no include that resolves into `src/`.
- [ ] Where a corpus needs something the scenario grammar cannot declare, the GRAMMAR gains it —
      not the door a second verb.

## What the measurement already says

A Khronos manifest declares exactly what a scenario declares: a glTF subject, a frame, and a
CAMERA that is derived from the framing rule the engine itself carries (`src/gltf/Framing.h`) --
the manifest quotes it so the runner can refuse a mismatch rather than trust one. `RenderPlan`
holds `Frame` and `Fill`; that is the same rule. **So a Khronos case needs no new grammar: it is
an asset plus a frame plus a fill.**

What the door still lacks for it is the PICTURE in the precision the oracle is kept in: the
oracle is EXR (float) and `Engine::Capture` writes PNG. A client that wants to compare against a
reference needs the frame as it was computed, not as it was quantised -- which is a real client
need and not a test convenience.

The other two are further away: test262 wants a scenario that declares a SCRIPT and reports
whether it threw; WPT wants one that declares a document and a stylesheet and reports the boxes.
Both are grammar, not door.

## Comments

- 2026-08-25 — filed after moving Script, Json, Markup, Style and Layout into the public door and
  moving them back: exposing the parsers is the wrong answer. The engine loads a scenario; a
  client never sees the reader.