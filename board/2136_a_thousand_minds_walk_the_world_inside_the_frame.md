Type: feature
State: open
Area: actor, engine, scenario
Tags: architecture, owner, ai-first
Depends: 2127, 2133, 2130

# A thousand minds walk the world inside the frame, each a programme or a prompt

**Benchmark** -- Unreal: `MassEntity` runs crowds as data-oriented fragments over `ZoneGraph`
lanes, with animation and physics LOD by distance -- a far agent is a position, a near one a
skinned body; behaviour is a state tree per agent. RAGE: ped pools with scripted brains and
task trees, streamed by distance, the same three rungs. **Both agree** on the SHAPE: an agent
is a row in a table, its rung is its distance, and its mind runs at a rate the rung allows.
Where the references have a scripter, this tree's `Scenario::Mind` already says what it has
instead: a `Programme`, or a `Prompt` and a `Model` -- an NPC whose mind is a language model
asked at `Hz`. **The choice is mine** for that last part, and it is the AI-first product.

## Where it stands, measured 2026-09-04

```
  Scenario::Mind      Tier, Uses, Programme, Prompt, Model, Meanwhile, Hz, EverySeconds -- declared
  the engine reads    Kinds and Instances (Assembly.cpp), a Script::Program (Declaring.cpp:63)
  Minds, Regions,     carried by Unacted() and acted on by nothing
  Doors
  bodies              Physics::Rigid, gravity only (board:2127); no skeleton runs on a body
  animation           glTF skins and clips import and pose (Live::Pose) for ONE subject
  crowds              Instances: placements of one shape; no per-instance pose, no per-instance mind
```

## The solution

- **an agent is a ROW**: `Column<Agent>` -- place, velocity, rung, body id, mind id, pose id --
  updated per step in one pass, which is the layout CLAUDE.md's frame-path rule demands
- **three rungs by distance**, the same number as board:2123's: far -- a placement on the
  network graph (board:2133), no body, no pose; middle -- a body on the physics with a posed
  skeleton at a reduced rate; near -- full rate, full contact
- **a mind is a programme or a prompt**: a `Programme` runs on the script engine at `Hz`; a
  `Prompt` is asked of its `Model` at `EverySeconds` through the host (`Host::calls`) and the
  answer arrives as EVENTS the agent's programme consumes -- so the frame never waits on a
  model, which is the fourth invariant applied to an LLM
- **skinning per instance**: the pose stream becomes per-instance rows, the placement table
  already is; animation LOD by rung
- **the snapshot** (board:2130) carries the agent table; the renderer reads placements and
  poses, never minds

## What will be true

- [ ] A scenario declares a `Kind` with a `Mind`, a thousand `Instances` of it, and a `Region`;
      they stand, walk the network and hold 16.7 ms at p99 at the 720p target
- [ ] A `Programme` mind and a `Prompt` mind both act: a case declares one of each and the
      body moves as the mind said
- [ ] The far rung costs a row; a case measures the step's time as linear in the near count and
      flat in the far count
- [ ] Deterministic: the same declaration replays the same crowd frame for frame; a prompt's
      answer is an event with a declared arrival, replayed from the log
- [ ] Negative control: run every agent at the near rung and the frame goes RED at a thousand

## What will show I was wrong

If a prompt-driven mind cannot be made deterministic through an event log, it is not a mind
the engine can replay, and CLAUDE.md's determinism rule puts it outside the engine -- a host
concern -- and the item says so.
