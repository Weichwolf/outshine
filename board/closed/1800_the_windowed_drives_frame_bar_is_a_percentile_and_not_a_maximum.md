Type: bug
Parent: 1778
Area: test, tools
Tags: frame, measured

# The windowed drive's frame bar is a percentile, and not a maximum

With `board:1778`'s budget arm the windowed drive finally reports instead of being killed, and
the first thing it reports is a red claim that nobody had ever seen evaluated:

```
NOTE the worst frame that laid no new corridor = 16.698 ms
FAIL and every frame that laid no new corridor is inside the 16.67 ms budget
```

**16.698 ms against 16.667 -- 0.19 % over, once, in 21 603 frames.** The distribution beside it:

| | |
|---|---|
| p50 | 1.725 ms |
| p95 | 2.705 ms |
| p99 | 4.075 ms |
| worst single frame | 16.698 ms at 1.401 km, frame 5022, steady |
| second worst | 16.102 ms at 0.001 km, frame 2 |

`CLAUDE.md` states the bar in the first paragraph of the file:

> *720p60 held -- **p50/p95/p99 over a moving camera, never a mean***

A MAXIMUM is not a mean, and it is not a percentile either. `worstSteadyMs` and `worstRelayMs`
are single-sample statistics over a population of twenty thousand: one scheduler hiccup, one
page fault, one compositor stall on a machine that is also running a browser, and the claim is
red while p99 sits at a quarter of the budget.

This is not an argument for a looser bar. It is an argument that the bar the tree DECLARES is
the one the case should assert, and that a maximum needs its own justification if it is to
stand beside it -- a frame that overruns by 0.19 % once is a different finding from a p99 that
overruns at all, and today they are the same claim.

Note also that frame 2 at 16.102 ms is a startup frame at 0.001 km. Whether the first frames
belong in the population at all is the second question this raises: `board:1601` established
that a slow test is a finding like a slow frame, and a warm-up excluded by name is honest where
a warm-up silently included is not.

## What will be true

- [x] The windowed drive asserts the bar `CLAUDE.md` declares -- p50/p95/p99 -- over the
      distance it drove, and publishes the maximum beside it as what it is: one sample.
- [x] If a maximum is to be a bar, it carries its own derivation and its own margin, and says
      what population it is a maximum over.
- [x] Startup frames are either in the population by name or out of it by name.
- [x] Proving test: the case itself, over its budget arm. Negative control: a frame budget set
      below p99 -> the percentile claim goes red rather than the single-sample one.

## Repaid (2026-08-24)

The maximum is no longer a bar. `CHECK(worstSteadyMs < 1000.0 / kFps)` is gone; what stands is
the p99 claim that was already beside it, and the maximum is published with its attribution:

```
NOTE p50 of the frame                          = 1.755 ms
NOTE p95                                       = 3.125 ms
NOTE p99                                       = 5.455 ms
NOTE the budget p99 leaves unspent             = 11.212 ms
NOTE the worst steady frame as a share of p99  = 5.066 x
WORST 22.389 ms at 86.555 km, frame 119126, t 2025.159 s, 146.7 km/h, moving, steady
```

**p99 spends a third of the budget and leaves 11.21 ms unspent.** The single worst frame is
5.07x that, and it can be looked at -- which is what an outlier is for.

The relay claim changed with it: `worstRelayMs < worstSteadyMs` rather than a second budget
bar. That is the comparison the claim's own prose was always making -- laying new road is
cheaper than the worst ordinary frame -- and it does not turn red because the machine hiccuped
on a frame that laid nothing.

- **Proving test**: the windowed drive over the whole route, 1/1 at `--timeout 900`.
- **Negative control**: `kFps` raised to 200, which is a 5 ms budget -- and it did NOT go red,
  because p99 fell to 4.665 ms on that run. That is the control failing to control, and it is
  recorded rather than hidden: the honest control for a distribution bar is a distribution that
  moves, and the one available here is `kLagsToCover`, whose own negative control
  (`board:1814`) drives the car off the road at km 113.990. **The p99 bar is not yet proven
  falsifiable on its own**, and that is this item's residue.

## The residue is closed: the p99 bar IS falsifiable, and the first control was simply too timid (2026-08-25)

The control that failed to control set `kFps = 200`, a 5 ms budget, against a p99 that landed at
4.665 ms on that run. It did not fail because a distribution bar cannot be falsified -- it
failed because 5 ms and 4.665 ms are the same number for this purpose, and a control has to
clear the measurement by more than the measurement's own spread.

`kFps = 1000` -- a 1 ms budget -- run over the whole route:

```
NOTE p50 of the frame              = 2.395 ms
NOTE p95                           = 4.825 ms
NOTE p99                           = 5.175 ms
NOTE the budget p99 leaves unspent = -4.175 ms
FAIL ...:498  **AND p99 IS INSIDE THE 16.67 ms BUDGET OVER THE WHOLE ROUTE.**
1 tests: 0 PASS  1 FAIL   in 459 832 ms
```

The claim goes red on the PERCENTILE, and the number beside it says by how much. Reverted to 60.

That closes the item's own residue in its own words -- *"the p99 bar is not yet proven
falsifiable on its own"* -- without needing `kLagsToCover` to drive the car off the road to do
it. A distribution bar is falsified by a budget below the distribution, and the control's job is
to be unambiguous rather than close.
