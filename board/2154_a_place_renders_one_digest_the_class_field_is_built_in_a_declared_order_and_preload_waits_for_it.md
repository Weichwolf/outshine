Type: bug
State: active
Area: world, engine
Tags: determinism, owner, audit

# A place renders ONE digest: the class field is built in a declared order and preload waits for it

**Benchmark** -- Unreal's automation compares screenshots bit for bit and calls a wandering one
a streaming bug; RAGE's replay plays a drive back frame for frame. CLAUDE.md's fourth invariant:
anything assembled from work on more than one thread is combined in a DECLARED order. Measured
2026-09-05 at HEAD (15406d39, before any change of this round): OldTown rendered 372d51ce,
7c1f32bd, 372d51ce in three runs, and 372d51ce, fa1af887, 7c1f32bd, 99f3ea05 over the day --
1 197 pixels, 273 by more than 1 of 255, worst 14 of 255, on the FAR GROUND (the hills behind
the town, looked at through a magnified crop), never on a building. The regression gate is
blind while this stands: a digest that moves on its own cannot attribute a change.

## Where it stands

- the one measure that differed between a 372d51ce run and a 7c1f32bd run, timings aside:
  `class field: the version the colours used` 3 against 4. The class builder is a worker with
  ONE pending job (the latest wins), so the number of builds before the picture is timing;
  both runs said `it calls itself complete` and held the same feature counts (7 457 / 7 415)
- `ClassField.cpp` sorted the job's features by rank alone with a STABLE sort, so equal ranks
  painted in the order their tiles landed -- completion order deciding overlaps. Repaired in
  this round: the sort key is (rank, the feature's own bounds, template, form, width, rings),
  never the landing. Nine runs after it read 372d51ce; the four before it did not all -- the
  repair is right and is NOT proved sufficient, because a fourth build was not caught since
- `Engine::settled()` (Engine.cpp:130) waits for tiles, rims and the stack's ingest and NOT
  for `Classes().Complete()`, so preload may return while the class builder still holds a job
  and the relay (`renamed` at Laying.cpp:297) lands inside the shot's frames, or after them
- a measure now digests the class structure's packed words (`class field: the structure's
  digest`), so the next run that reads version 4 says whether its CONTENT differs from
  version 3's -- the same features in a different job, or a job that missed a landing

## Measured 2026-09-05, after the sort: the class field is NOT the cause

With the structure's packed words digested, a version-4 run and a version-3 run of OldTown
read the SAME digest (877494161 / 342430996) and both rendered 372d51ce; the fourth build
changes nothing a shader reads. Yet c803c350 appeared once more in the same hour. The two
measures that differed between a 372d51ce run and a 7c1f32bd run, timings and the version
aside: `asks that repeated a posted job` (3 669 against 3 419) and `generators: and flora
wanted ground another body already held` (138 against 215). The suspect moves to the tile
pool: a repeated ask is a tile re-posted after an eviction or a miss, and a far sheet whose
finer rung had not landed by the shot's frame is drawn at a coarser one -- the far ground's
colours, exactly where the pixels moved. The next measurement: per run, the rung of every
sheet at the shot's frame, digested; a wandering run reads a different digest there or it
does not, and either answer names the next suspect.

## The solution

- preload is settled only when the class field is complete and the ground was laid at that
  version: `settled()` takes `Classes().Complete() && World.LaidClasses == version`
- the builder's job is a snapshot of the landed tiles in DECLARED order (tile id), and a
  landing after the job was taken marks it stale so that one more build follows -- the
  `Stale` flag's hole, if the version-4 digest differs, is here
- a case: OldTown rendered N times through the client reads one digest; its negative control
  is the stable sort by rank alone, which reads two

## What will be true

- [x] the features paint in a declared order
- [x] the class structure's digest is the same across every run that says complete (and
      so the field is not the cause; the pool's rungs at the shot's frame are next)
- [ ] `settled()` waits for the class field, and a place shot right after preload reads the
      same digest as one shot a second later
- [ ] ten runs of OldTown, Heidelberg and Kaiserberg read one digest each
- [ ] Negative control: the rank-only stable sort put back reads more than one digest in ten
