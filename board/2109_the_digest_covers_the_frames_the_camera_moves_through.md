# The digest covers the frames the camera moves through

State: open

`make shots` renders one still view per place, digests THAT, and then renders 120 more frames along
a walk to time them. Not one of the 120 is digested.

So the determinism proof -- "the same declaration renders the same bytes, twice" -- covers a camera
that is standing still, and says nothing about the camera that moves. Which is backwards: a still
frame streams nothing, changes no level of detail, and waits on no arrival. The 120 frames are
where a tile lands a frame later, where an LOD flips, where a fold takes completion order instead
of declared order. Every failure mode goal 3 names lives in the frames nobody hashes.

Measured on 2026-09-03: `WalkedTo` used the LONGITUDE metres-per-degree for the LATITUDE step --
111320 where 111132 is the figure -- so every walk in every place ran 0.17% long on its north
component. Correcting it moved NO digest, because no digest sees a walked frame. A defect on the
walk is invisible to the gate that exists to see defects.

**Benchmark**: Unreal's automation compares screenshots frame by frame along a recorded camera path
and calls a wandering one a streaming bug -- the path is the point, not the pose. RAGE's replay
plays a drive back frame for frame and expects the same frames. They agree completely, and both do
the thing this tree does not. Taken: theirs.

**Closes when** `make shots` writes a digest per walked frame, or one digest OVER the walk (a hash
of the 120 hashes, which keeps the trailer short), and three runs of `make shots --all` agree on
it. The still frame keeps its own digest -- it is the one a human looks at.

**The measurement that shows I was wrong:** if the walk's digest differs between two runs on this
machine on the first day, then the walk is not deterministic today and this item has found a defect
rather than a gap. Either outcome is worth the change; a green one means the cover was missing, a
red one means the cover was missing AND something was already wrong under it.
