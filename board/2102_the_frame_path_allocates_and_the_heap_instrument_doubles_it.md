Type: bug
State: withdrawn
Area: base, compositor, engine
Tags: measured, performance, owner

# Nothing on the frame path allocates, and the heap's own measure costs nothing

**Benchmark** -- RAGE: `sysMemAllocator` keeps the block's size in a HEADER before the block, so
returning it is a subtraction and never a question to the allocator; pools are pre-sized and the
frame path takes from them. Unreal: `FMalloc::GetAllocationSizeUntyped` exists exactly because the
engine refuses to ask the platform allocator on the hot path, and `FMemStack` hands out frame
scratch with a bump pointer that is reset once per frame. **Both agree**: on the frame path an
allocation is a pointer bump, and knowing how big it was is free.

## Measured 2026-09-02, Kaiserberg, `shots --measures` and `sample`

`rebuild: of that, the streets and the water` reads 11 363 ms. Timing each pass of it separately,
which this round added:

| pass | ms |
|---|---|
| finding the crossings | 830 |
| raising the decks | 50 |
| shortening the ends | 8 |
| designing every lane | 142 |
| levelling the junctions | 29 |
| **paving every lane** | **6 288** |

**AND THE PROFILE NAMES SOMETHING ELSE AGAIN.** An eight-second `sample` over the same build, 1 349
samples inside `Grounds`:

| | samples | share |
|---|---|---|
| `YieldGround` | 872 | 65 % |
| of that, `Heap::Take` and below | ~240 | 28 % of `YieldGround` |
| `Cut(...)::$_0` | 74 | |

**TWO HYPOTHESES WERE WRONG BEFORE THIS ONE and both are recorded so the next reader does not
repeat them.** First: `atNode`, the junction map, was rebuilt inside the 24-pass levelling loop
although its contents never change -- hoisting it out is correct and saved 117 ms of 11 236, which
is one per cent. Second: `Drape::At` is called 2.7 MILLION times per rebuild (916 k designing,
1.82 M paving) and walks 89 million faces; at the 4.4 ns per face the design pass measures, that is
about 250 ms, not six seconds. Neither was the bottleneck. The profile was.

## THE HEAP'S MEASURE COSTS AS MUCH AS THE ALLOCATION

```
Heap::Take  -> malloc
            -> BlockBytes(block) -> malloc_size()   a metadata walk inside libmalloc
            -> two atomic fetch_add
            -> RowFor(gTag)                          a lookup by tag string
Heap::Return                     -> malloc_size()    again
```

In the sample `malloc_size` carries roughly as many samples as the `malloc` it follows. The
accounting is on the hot path and it doubles the cost of the thing it accounts for. `Take` already
KNOWS the byte count it was asked for; it calls `malloc_size` because `Returned` does not, and the
two must agree.

## What will be true

- [ ] `YieldGround` allocates nothing per yield and nothing per cut: its buffers are held across
      the call and cleared, the way `Paved`'s now are
- [ ] The heap's accounting asks the allocator NOTHING. Either the size rides in a header the way
      RAGE does it, or the count is of bytes ASKED rather than bytes given and `Returned` is told
      the size
- [ ] `rebuild: of that, the streets and the water` is quoted before and after for Kaiserberg,
      Heidelberg and Shibuya, and the number becomes a ceiling that may only fall
- [ ] `heap taken under world-ground` -- 11.6 GB of CHURN for one rebuild at OldTown -- is quoted
      the same way
- [ ] Negative control: put one allocation back inside `Cut`'s loop and require the ceiling to go
      RED

## What is NOT in this item

Whether `Drape::At` should be a BVH query instead of a six-rung hash scan. It is not the
bottleneck: measured at 4.4 ns per face walked, and 89 M faces is a quarter of a second. It is
written down so the next reader does not spend a day on it.
