Type: bug
Area: sim
Tags: hot-path, layout, measured, drive

# The tick publishes its tally where it lives, and does not copy it every tick

`DriveTick` accumulates into persistent state and then returns a **copy of the whole
accumulator** on every call:

```cpp
src/sim/DriveTick.h:108   [[nodiscard]] Ridden DriveTick(const Corridor &way, const Rigged &stood,
                                                         DriveState &drive, double dtS, const Taken *taken);
src/sim/DriveTick.cpp:40    Ridden &out = drive.Tally;
src/sim/DriveTick.cpp:66,261,267,274   return out;
```

`out` IS `drive.Tally`. The caller already owns `drive`, so the return value carries nothing
the caller cannot read, and it carries it by memcpy.

## Measured, at 2b2c2f69, this machine

```
sizeof(Ridden)     = 2440 bytes
sizeof(DriveState) = 4880 bytes      (it embeds a Ridden Tally)
```

`Ridden` was ~288 bytes before this session. `board:1812` added the deviation histogram
(`uint32_t OffsetBin[512]`, `src/sim/DriveTick.h:37-40`) and `board:1767` added a further
twelve attribution doubles, so a per-tick return value grew **8.5x** to hold a monotonic
accumulator that never needed returning at all.

Over the shipped Munich--Hamburg drive, which reports **2 791 050** offset samples, that is
2 791 050 x 2440 B = **6.8 GB of memcpy** whose only purpose is to hand back a struct the
caller has a reference to.

## Why this is architecture and not micro-optimisation

CLAUDE.md's CURRENT map already colours `DriveTick` amber for exactly this shape -- *"returns
a whole struct by value each tick"* -- and the shape is now a per-frame 2.4 kB copy in
`apps/driver/test/window/AWindowShowsTheRoadTheCarIsDriving`, which ticks the drive inside its
frame loop. "Bounded terms on the frame path" is not satisfied by a bound that grows every time
someone adds a counter, and a struct nobody may cheaply return is a struct that will be
returned anyway until the signature stops offering it.

## What will be true

- [x] The tick writes its tally and the caller reads it: `void DriveTick(...)` with the
      accumulator read as `drive.Tally`, or a `const Ridden &` return -- either way no copy.
- [x] The histogram is a named type beside its bins with its own `[[nodiscard]] double
      Quantile(double) const`, not 512 raw `uint32_t` plus quantile arithmetic re-derived in
      each case (`apps/driver/test/APlannerFindsTheRoadFromMunichToHamburg.cpp:149-160` is the
      only implementation today, and the window case would need a second).
- [x] `static_assert(sizeof(Ridden) <= N)` stands beside the struct, so the next counter that
      doubles it is a compile error rather than a measurement nobody takes.
- [x] Proving test: a unit case in `test/unit/sim/` that ticks a synthetic corridor N times and
      asserts the bytes moved per tick. Negative control: the by-value signature restored ->
      the number is 2440 and the case names it.

## Repaid (2026-08-24)

Two changes, and the second is the structural one.

**The histogram is an accumulator, so it lives in the state.** `OffsetBin[512]` and its bin
width moved from `Ridden` to `DriveState` -- 2048 of the 2472 bytes were a route-long tally
sitting in a per-tick answer.

**And the answer is handed back rather than copied.** `DriveTick` returns
`const Ridden &` -- it always was `Ridden &out = drive.Tally;`, so the caller already owned the
object and the value return copied it back to itself once per tick.

```
sizeof(Ridden)  2472 -> 424 bytes
copies per tick    1 -> 0
```

Over the shipped drive's 2 791 050 ticks that is **6.9 GB of memcpy** that no longer happens.

Three `static_assert`s stand beside it now, where none did:

```cpp
static_assert(sizeof(Ridden) == 424, "sizeof(Ridden)");
static_assert(std::is_trivially_copyable<Ridden>::value, "a tick answer is a value");
static_assert(sizeof(DriveState) >= sizeof(Ridden), "the state holds the tally");
```

- **Proving test**: the whole drive suite, `apps/driver/test` 5/5 at `--timeout 900`, and
  `unit/sim` 7/7 in the fast gate.
- **Negative control**: a `double` added to `Ridden` -> the size assertion fires at compile time
  with the size it found, the way `board:1781`'s does for `Forest::Stem`.
- What stays open and is the review's own reading: `Ridden` is BOTH a per-tick answer and a
  route-long tally. Splitting them is a shape question, not a cost one, and the cost is paid.
