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

- [ ] The windowed drive asserts the bar `CLAUDE.md` declares -- p50/p95/p99 -- over the
      distance it drove, and publishes the maximum beside it as what it is: one sample.
- [ ] If a maximum is to be a bar, it carries its own derivation and its own margin, and says
      what population it is a maximum over.
- [ ] Startup frames are either in the population by name or out of it by name.
- [ ] Proving test: the case itself, over its budget arm. Negative control: a frame budget set
      below p99 -> the percentile claim goes red rather than the single-sample one.
