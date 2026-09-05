Type: feature
State: open
Area: engine, actor, scenario
Tags: architecture, owner, ai-first, determinism
Depends: 2136, 2130

# A mind is a PROVIDER with three verbs -- perceive, act, remember -- and every answer replays

**Benchmark** -- Unreal: an AI is a behaviour tree over a blackboard fed by a perception
component (`AIPerception`: sight, hearing, with a sense's range and age), acting through
`AIController` verbs (`MoveTo`, `Focus`); RAGE: a ped's brain is a task tree over a scripted
perception, and its REPLAY records every input so a drive plays back frame for frame. **Both
agree** that perception is a SNAPSHOT the AI reads, action is a small verb set the engine
executes, and the mind's decision is not on the frame. Where the references have a scripter,
this tree has a `Prompt` and a `Model` (`Scenario::Mind`) -- the AI-first product, **the choice
mine**, and CLAUDE.md's fourth invariant makes the rest compulsory: a language model is IO, it
answers late and never twice the same, so it runs on the IO pool and its answers are EVENTS.

## Where it stands, measured 2026-09-05

```
  Scenario::Mind           Prompt, Model, Hz, TokenBudget, LatencyBudgetMs, Temperature, Seed
                           declared, acted on by nothing               Scenario.h:292-306
  perception               no snapshot a mind can read; the engine's snapshot is the
                           renderer's (board:2130)
  action                   no verb set; a body moves by physics or by a view's input
  memory                   nothing persists per mind
  the IO pool              exists for fetches (TilePool carriers); no provider speaks to a model
  replay                   `make shots` digests a picture; no run records its inputs
```

## The solution

- **perceive**: the engine writes, at the mind's `Hz`, a TEXT snapshot of what stands within its
  sense range, in the names the Earth's data gives them (the OSM name of the street, the kind of
  the building, who else stands here, the clock, the weather) -- the same snapshot rule as the
  renderer's, one step behind the simulation and never a live read
- **act**: a closed verb set -- `walk(to)`, `say(what, to)`, `give`/`take(item, whom)`,
  `trigger(event)`, `wait` -- parsed from the answer, CHECKED against the declaration (the item
  exists, the mind holds it, the place is reachable on board:2133's graph) and executed by the
  engine over frames; a refused act is a refusal the mind reads in its next snapshot
- **remember**: a per-mind store the prompt carries forward, bounded by `TokenBudget`
- **provider**: the model is a `Provider` on the IO pool with `LatencyBudgetMs`; a mind acts on
  its last snapshot while the answer is on the wire; the answer lands as an EVENT stamped with
  the simulation step it was asked at
- **replay**: every answer is appended to the run's event log; a run declared with that log
  reads its answers from the log instead of the wire, so the same declaration plus the same log
  renders the same bytes -- RAGE's replay, with the model taken out of the loop

## What will be true

- [ ] Three minds in one place perceive, act and remember through the door; the picture shows
      them walk and the tables show what they gave
- [ ] A run with the event log replays bit for bit: same digests over N steps with no model
      reachable (the negative control is the same run with the log withheld: it REFUSES rather
      than asks, because a replay that asks is not a replay)
- [ ] The frame never waits on a model: with `LatencyBudgetMs` set to ten seconds, `the step's
      own time, most` reads what it reads without minds
- [ ] An act the declaration forbids is refused and the refusal is in the next snapshot; a
      case declares a mind that tries to give what it does not hold and reads the refusal
