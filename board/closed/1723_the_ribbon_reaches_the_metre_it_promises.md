Type: bug
Area: actor
Tags: correctness, geometry

**The ribbon reaches the metre it promises**

`Sweep` (src/actor/path/Ribbon.cpp:44) takes `stations = (size_t)((toM - fromM) / stepM) + 1`
and stations at `fromM + station * stepM`. Off the grid, the last station sits at
`floor((toM-fromM)/stepM) * stepM` — up to a full step SHORT of `toM` — while `out.ToM = toM`
(line 25) claims the coverage. The clamps at lines 64, 77, 87 and 130 (`atM > toM ? toM :
atM`) guard an overshoot this formula cannot produce: they are the fossil of the intended
extra station, the same fossil 1715 dug out of SpeedProfile the same hour. The road mesh ends
early, the end cap (line 129) seals the wrong station, and whatever abuts at `toM` — the next
ribbon, a junction apron — meets a gap of up to `stepM`.

Demanded: an off-grid range takes one MORE station clamped to `toM` (making the clamps live,
exactly the 1715 fix), and the ribbon's unit twin proves the last surface vertex row stands
at `toM` for a range that is not a multiple of `stepM`.

---

Closed -- an off-grid range takes one more station clamped to toM (the 1715 fix, here), so
the clamps are live and the surface reaches the metre ToM promises. Proven in
ARibbonIsClosedAtBothEnds: 100..500.7 at step 2 puts the last vertex row at 500.7 (the
floor-count sweep ended at 500 -- negative control red on exactly this arm).
