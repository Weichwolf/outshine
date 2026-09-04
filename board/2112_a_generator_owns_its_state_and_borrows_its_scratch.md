# A generator owns its state and borrows its scratch

State: withdrawn

A generator needs two kinds of memory and this tree only names one of them.

  FIXED     what does not change between calls -- the species, the density table, a prototype.
            The generator OWNS it, it is built once, and it is `const` while the frame runs
  SCRATCH   what belongs to one call and is recycled -- the working buffers a mesher fills and
            throws away. The CALLER owns it, because the caller knows how often it calls and
            whether it calls in parallel

The fixed half is right: `Forest` holds its stems in an array and its densities in a vector of its
own. So does `OccupancySink`, whose `Storage` takes `std::span<Solid>` and `span<uint32_t>` from
the caller -- exactly the borrowed-scratch shape, in the one place this tree already does it.

The meshers do not. `BuildingMesh` declares NINETEEN local `std::vector` per call and `RoadMesh`
eight, and `Mesh` is called once per building: 1275 buildings in OldTown, so roughly 24 000
allocations for one region. The seam gives them nowhere else to put it -- `Mesh(plan, into) const`
has a plan and a result and no working memory -- and a `mutable` member would be worse, because it
would bind the mesher to one thread.

**AND IT IS NOT A PERFORMANCE FINDING, which is why the arithmetic is here rather than a claim.**
24 000 allocations at ~100 ns is 2.4 ms. OldTown's p99 is 2267 ms. That is 0.1%, so anyone who
sells this as the fix for the frame budget has not done the division. What it IS: an invariant --
CLAUDE.md forbids allocation on the frame path and meshing runs there while tiles stream in -- and
the thing that makes a mesher safe to call from two workers at once.

**Benchmark**: Unreal gives it `FMemStack` with `FScopedMemMark`: a stack allocator RESET on scope
exit rather than freed, plus `TArray::Reset()` which keeps capacity. RAGE runs frame heaps through
`sysMemAllocator` on the same principle -- never free, only reset. They agree, so the matter is
closed: **scratch is reset, never freed, and it is passed in.**

Taken: the seam grows a scratch parameter, `Mesh(plan, scratch, into)`, matching what
`OccupancySink::Storage` already does. The caller holds one per worker.

**The part that is easy to get wrong:** recycled memory must be RESET and not merely reused. A
scratch carrying remains of the previous call makes the result depend on call ORDER, which is the
same defect as a counter that drains -- and goal 3 forbids exactly that.

**The measurement that shows I was wrong:** if the meshers' allocations do not fall to zero per
call after the change, the scratch is not covering what they actually use. Count with a tagged heap
before and after, on the same place.
