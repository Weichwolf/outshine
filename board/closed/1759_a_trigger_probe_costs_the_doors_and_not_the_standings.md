Type: bug
Parent: 1488
Area: scenario
Tags: performance, frame-path, unbounded, telemetry

**A trigger probe costs the doors and not the standings**

`TriggerField::Probe` (src/scenario/Triggers.cpp:135-165) is the frame path — one call per
MOVING body per tick. Inside the loop over doors it scans EVERY standing pair to find this
body's:

```cpp
for (uint32_t which = 0; which < (uint32_t)Doors_.size(); ++which) {   // :143
  ...
  for (size_t at = 0; at < Inside_.size(); ++at) {                     // :147
    if (Inside_[at].Body == body && Inside_[at].Door == which) { standing = at; }
  }
```

`Inside_` holds ALL (body, door) pairs of ALL bodies — up to `kMostStandings` = 4096
(:13). So the per-tick term is

    moving bodies × kMostDoors × kMostStandings = bodies × 256 × 4096

and the loop does not even break on the match (:147-149 keeps scanning after finding it).

**Measured** (probe against src/scenario/Triggers.cpp at HEAD, `-O2`, this machine, 200
bodies, 60 ticks, boxes centred so every body stands in every door — the standings pool
saturates at its declared 4096):

| doors | ms per tick, bodies inside | ms per tick, bodies outside | ratio |
|---|---|---|---|
| 8 | 1.116 | 0.004 | 279 |
| 32 | 6.998 | 0.014 | 500 |
| 128 | 34.500 | 0.045 | 767 |
| 256 | 66.680 | 0.089 | 749 |

At the field's OWN declared pool bounds the tick costs 66.68 ms — **four times the whole
16.67 ms frame** (720p60 = 1000/60 ms) — while the same configuration with nobody inside a
door costs 0.089 ms. A term that swings by 750× with the data is not a bounded term; it is a
cliff the scenario author falls off by declaring the pools the engine advertises.

`board:1488`'s progress note claims "the per-tick term is O(moving bodies x declared doors),
never O(instances)". The measurement above says the third factor was left out of the claim.

## The second half: the standings overflow is silent and self-amplifying

```cpp
if (in && standing == Inside_.size()) {
  if (Inside_.size() < kMostStandings) { Inside_.push_back(...); }   // :151-153
  if (door.Opens == When::Enter) { fire(door.Event); }               // :154
```

When the pool is full the standing is DROPPED with no counter — `Overflowed_` (:139) counts
the fired-ring overflow only. The body then has no standing, so the next tick takes the same
branch again: it **fires Enter every tick, for ever**, and never fires its Exit. A dropped
standing is not a degradation, it is a corrupted event stream, and nothing publishes a
number that would show it.

## What will be true

1. The standing lookup is O(1) or O(doors this body stands in), not O(all standings) —
   e.g. a per-door bitset over body ids, or `Inside_` sorted by (body, door) with a
   fixed-capacity open-addressed index built at `Build`. No allocation, no growth.
2. The frame-path term is proven: a unit case stands the field at its DECLARED bounds
   (`kMostDoors` doors, `kMostStandings` standings) and asserts the per-tick cost against a
   stated fraction of the 16.67 ms frame — a test that fails at HEAD by 4×.
3. A dropped standing is a NUMBER (`StandingsDropped_`), published beside `Overflowed_`, and
   the engine does not re-fire Enter for a body whose standing it refused to record — either
   the pool refuses at `Build` (bodies × doors is knowable there) or the drop is a
   suppression, not a repetition.
4. `kMostDoors` / `kMostStandings` / `kMostFired` carry their derivation and population the
   way the tree's other `[SET]` values do; today (:11-14) they carry a sentence and no number,
   and the sentence is what the measurement above contradicts.

---

Closed -- the standings are indexed BY DOOR (one list per door, each reserved at its own
bound), so a probe walks the few bodies standing in THAT door and breaks on the match; the
(body, door) pair scan that cost doors x standings is gone. Measured on the item's own
population, 200 bodies inside 256 doors: 0.18 ms a tick where the pair scan cost 66.68 ms
-- four frames for one tick at the pool's own declared bounds. kMostStandings is a per-DOOR
bound now (256, with its derivation: more than that standing in ONE door is a crowd no
scenario declares). The second half is repaid too: a body the pool cannot seat is COUNTED
(Unseated()) and does NOT fire -- it would otherwise have fired Enter every tick and never
Exit, because it had no standing to exit from. Proven in AVolumeFiresAndSomethingHears
(crowd arm: under a quarter frame, zero allocations, nothing unseated); negative control:
restoring the pair scan turns exactly that arm red at 9.99 ms.

And 1488's progress note is corrected by this closure: it claimed "O(moving bodies x
declared doors)" and "overflow counted" when the code did neither. The note was mine and it
was wrong.
