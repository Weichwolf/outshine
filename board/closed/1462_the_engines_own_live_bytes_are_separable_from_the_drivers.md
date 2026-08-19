Type: feature
Area: core
Tags: instrument, perf

**The engine's own live bytes are separable from the driver's**

A long run reports what THIS ENGINE is holding, so a leak in it is a number with a cause. `HeapProbe`
answers over `malloc_default_zone`, which is every library in the process; a counter kept by the
engine's own `operator new` and `operator delete` answers over the engine.

## What it costs today, measured rather than supposed

`test/scenario/ALongRunHoldsItsMemoryAndItsPace.cpp` runs 600 frames and can only claim a ceiling,
because the population it reads includes the device.

| run | highest reading above the settled floor |
|---|---|
| `Box`, a subject that submits nothing after its first frame | **3 191 696** |
| the same, again | 3 187 680 |
| the same, again | 3 194 256 |
| the same, again | 3 196 816 |
| `BoxAnimated`, posing and resubmitting every frame | 2 388 880 |

**A spread of 9 kB over 3.19 MB is a deterministic arena, and the arm that does LESS work takes a
BIGGER one** -- so the step is the driver's and the engine's own growth is somewhere underneath it,
unmeasured. The instrument's floor is therefore about **8 kB a frame**; anything smaller is invisible.

## THE HORIZON IS HUNDREDS OF HOURS, AND IT IS WHAT MAKES THIS EXACT RATHER THAN STATISTICAL

**Suspend and quick resume mean one process runs for a console's whole relationship with a game.** The
owner named it and it settles the design: a hundred hours at the frame budget's own rate is
`100 * 3600 * 60` = **21 600 000 frames**.

| leak per frame | over a hundred hours | on an 8 GB device |
|---|---|---|
| **1 byte** | 21.6 MB | survivable |
| 32 bytes | 691 MB | a different game by the end |
| 100 bytes | 2.16 GB | dead |
| 1 kB | 21.6 GB | dead in the first four hours |

**NO RUN LENGTH THIS SUITE CAN AFFORD REACHES THAT HORIZON**, so the instrument cannot be statistical.
A 600-frame run sees a 1-byte-a-frame leak as 600 bytes, which is four orders of magnitude under the
driver's own arena and would be invisible at any sampling rate.

**THE ANSWER IS EQUALITY AND NOT A SLOPE.** A steady-state scenario runs the same code on every frame,
so an engine-owned counter read at the SAME point of two consecutive frames must return **exactly the
same number** -- not close, equal. Any difference is bytes taken and not returned, and the resolution
is one byte. **A leak is then found in six hundred frames and its consequence at 21 600 000 is
arithmetic**, which is the only way a claim about a hundred hours can be made in a suite that runs for
twenty seconds.

*That is why this item is a counter and not a longer run: the run length was never the problem.*

## THE ESTABLISHED ANSWER IS NOT A LEAK DETECTOR, AND THAT CHANGES THIS ITEM

**Nobody who ships an engine hunts leaks on the frame path; they make one unspellable there.**

| | how it is done, and it is thirty years old |
|---|---|
| **Unreal** | `FMemStack` -- a linear stack allocator whose items are pushed and then **freed en masse** by an `FMemMark` popping the whole frame at once. Per-frame work allocates from it and nothing is individually returned |
| **RAGE** | **fixed pools sized at build time**, declared in `gameconfig.xml`. Exceeding one does not grow the heap; the game refuses. The budget is a declaration and the allocator has nothing to decide |

**`CLAUDE.md` ALREADY SAYS THIS AND IT IS ONE OF THE SIX**: *the frame path is made of bounded terms --
every step in it costs a number somebody can name, which is exactly what an allocation, a block, a lock
that might wait and a disk touch are not, so those four live at load*. The field and this repository
agree; what was missing is an instrument that can tell whether the tree obeys its own rule.

**SO THE CLAIM IS ZERO AND NOT *NO GROWTH*.** A frame that allocates nothing cannot leak, cannot
fragment and cannot stall in an allocator, and equality between two frames follows by construction
rather than by measurement. This repository already holds that standard in one place -- the script tick
is measured at **0 bytes over 20 000 ticks** -- so the bar is set and the frame path is simply not held
to it yet.

## THE MEASURED VIOLATION, and it is in code written this session

[MEASURED] the live zone churns **1.33 MB frame to frame** over an animated scenario. `Clients::Live::Pose`
rebuilds `Gltf::Subject` on every advance and copies the whole previous pose beside it, and
`Clients::Show` refills its scratch vectors -- so an animated frame takes and returns a megabyte on a
path `CLAUDE.md` says may take nothing.

**It is not a leak and it is not free**: an allocator's take-and-return is work, it fragments, and it is
the term that makes a p99 spike where a p50 looks well. *A still scenario already costs 0.0654 ms a
frame against an animated one's 0.3154, and this is where the difference lives.*

## Where the mature answer must NOT be copied whole, and the reason stands beside it

**RAGE's pools are sized at BUILD time because its content is known at build time.** This engine loads
its world from OSM and generates the rest, so a fixed pool per asset class is a budget nobody can write
down. What transfers is the discipline -- **declared capacity, reused storage, a refusal instead of a
growth** -- and what does not is the fixed table. That is the deviation and this paragraph is its reason.

## What must be true

- [x] **`operator delete` is replaced beside `operator new`**, so both sides of a block are counted. It
  is the one shape that can answer *what is live*, and `Heap.cpp` currently replaces only the taking
  half and says so
- [x] **The size of a freed block is read rather than remembered** -- `malloc_size` on this platform --
  so no table maps a pointer to a size and no map is a second allocation on the free path
- [x] **The counter is a relaxed atomic and the frame path does not read it.** Counting is two adds; a
  sample is a load, and neither walks anything
- [x] **Both figures are published side by side** and neither is called the other: the process's zone
  says what the machine holds, the engine's counter says what this repository is responsible for
- [ ] **The frame path allocates ZERO bytes**, which is the claim the counter exists to decide -- a pose
  writes into storage grown once at stand-up, and a subject that changes size refuses rather than grows
- [ ] **The long run then judges EQUALITY and not a ceiling**: after the settling frames, the engine's
  own live bytes at one point of the frame are the same number on every frame, and a difference is
  reported in bytes and in what it becomes over 21 600 000 of them
- [ ] **A scenario that legitimately grows says so** -- a stream-in holds what it streamed -- so the
  claim is over a scenario in STEADY STATE, declared as one, and never over every run by default

## What this feature may NOT do

**It may not become a per-allocation ledger.** A map from pointer to size on the free path would put an
allocation inside a deallocation, which is the shape `CLAUDE.md` forbids on the frame path and a
needless cost everywhere else.

## What it answered the moment it existed

[MEASURED] `BoxAnimated`, 500 frames, 250 of them settling, over the subject's own 223-frame lap:

| | |
|---|---|
| the engine's own live bytes | 2 159 488 at the settling point, 2 159 392 at the end |
| pose-matched pairs one lap apart | 27, of which **19 differ**, worst **416 bytes** |
| the mean difference | **-0.4783 bytes a frame** -- NEGATIVE, so it oscillates and does not leak |
| over 21 600 000 frames | -9.9 MB, which is *not a leak* said in the horizon's own units |
| **frames whose live bytes moved at all** | **172 of 249** |

**THE LAST ROW IS THE FINDING AND THE FIRST FOUR ARE WHY IT IS NOT A LEAK.** The engine takes and
returns memory on two frames in three, and the taking and the returning cancel -- so nothing grows, and
the whole of it is work a shipped engine does not do.

*The pose-matched comparison is what made that readable. Comparing CONSECUTIVE frames reported 4.48
bytes a frame over a run where 183 of 250 read differently from the one before -- which is an
animation's own poses sizing containers differently, not a leak, and would have been filed as one.*

## Comments

**The round that filed this nearly filed a leak instead.** Its first metric -- the highest reading minus
the settling point -- reported 758 144 bytes of growth over a run whose lowest reading was 569 408 bytes
BELOW its own start. The second -- the floor of the last quarter against the first -- changed SIGN
between two runs of one declaration. **A metric whose sign flips between runs of the same declaration
decides nothing**, and the still-subject control is what turned the question from *is the engine
leaking* into *whose bytes are these*.
