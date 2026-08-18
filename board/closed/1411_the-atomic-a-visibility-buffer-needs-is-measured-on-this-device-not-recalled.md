Type: task
Parent: 0079
Area: render
Tags: instrument, perf

**The atomic a visibility buffer needs is measured on this device, not recalled**

`src/core/ClusterDag.h` opens by naming itself *Nanite half 1* and says of half two -- the compute
software rasteriser -- that it *cannot be* here, because *a shading language has no 64-bit atomic*.
**A sentence that closes a whole technique is worth one test**, and it was a sentence rather than a
measurement.

**A software rasteriser resolves visibility by writing `(depth << 32) | primitiveId` into a per-pixel
word with an atomic max**: the nearest fragment wins and its identity arrives with it, in ONE operation
with no lock and no second pass. Split across two 32-bit words it is not atomic at all -- two threads
interleave and a pixel ends up with one fragment's depth and another's identity.

## What it measured

[MEASURED] on this device, Metal through SDL_GPU, at the same runtime path every shader in this tree
takes:

| probe | verdict |
|---|---|
| **32-bit `atomic_fetch_max` -- THE CONTROL** | compiles |
| 64-bit `atomic_fetch_max` on `device atomic_ulong *` | **refused** |
| 64-bit `atomic_compare_exchange_weak` | **refused** |

The compiler names the unsatisfied trait outright: `_valid_fetch_max_type<device unsigned long *,
void>`. `atomic_load_explicit` on the same pointer is refused too, so it is the WIDTH and not the
operation.

**The control is what makes the answer mean anything**, and it is the reason this item is not just the
sentence again: the same shader with the word narrowed to 32 bits and nothing else changed compiles,
so a refusal of the wide one is about the width and not about the probe.

## What is asserted and what is reported

**Only the control is asserted.** The engine does not depend on the wide atomic today, so a red would
be a permanent one about a capability nobody is waiting on. **The answer is a `NOTE` the suite re-takes
on every run** -- so the day a driver or an SDL version changes it, the note changes and a closed
technique reopens by itself.

## Comments

**The owner's reading was that the limit came from WebGPU and would not apply on Metal. The device
refuses it on Metal.** *Which is the whole argument for exercising a capability rather than reasoning
about it -- and the reason the note is re-taken rather than written down once.*

**What is NOT established**: whether this is the GPU family or the Metal language version SDL_GPU
requests when it compiles MSL at run time. `SDL_GPUShaderCreateInfo` carries no version field, so
under SDL_GPU as it stands the answer is the same either way -- but they are different facts and only
one of them is about the hardware.
