Type: bug
State: open
Area: test
Tags: measured

# The fast gate is fast again

**Benchmark** — Unreal: a smoke suite runs on every submit and the full automation runs nightly; the split is by TIME, not by taste. RAGE: the same shape. **They agree**, so the matter is closed: a gate has a time budget, and what fits is a decision made against that budget rather than a list that grows.

CLAUDE.md states the budget: `test/gate.sh` is the fast gate, under a minute. Measured, warm:

    the library and its clients                3 s
    the tiers and what stands wider            1 s
    khronos static                            50 s
    khronos animated                          11 s
    the simulation, content, mix              19 s
    the places on Earth                       36 s
    -------------------------------------------
    GREEN in                                 121 s

Cold it is 254 s. So the gate is TWICE its budget warm and four times it cold, and the promise on
its own page is false.

The places were added on the owner's instruction -- every gate drops six pictures into
`build/places/` so the engine's visual state is visible -- and they are 36 s of the 121. The khronos
pair is 61 s for six cases, which is link and run rather than compile, because the gate has already
built the library by then.

## The choice this item must make, and it is the owner's

1. **the budget moves** to what the content is worth, and CLAUDE.md's line changes with it
2. **the content shrinks**: khronos static and animated leave the gate and are named as an
   obligation the way the claims already are -- "any change under src/, include/ or apps/ wants
   test/run.sh harness/claims". That would put the gate at 60 s
3. **the suites run concurrently.** `run.sh` refuses two runs in one nest, deliberately, because two
   gates in one nest read each other's half-written objects. Lifting that is a change to the runner
   and not to the gate

## The measurements that would show I am wrong

1. **The 50 s is not compile.** The gate runs `make` first, so the library stands; if khronos static
   still costs 50 s after a second consecutive gate, the time is link and run and option 2 buys the
   full 50 s. If it falls, the number is a cache miss and the item is misfiled
2. **The places are not the problem.** 36 s of 121 is under a third, so removing them would not
   reach the budget either -- which is the argument against the one change nobody wants
