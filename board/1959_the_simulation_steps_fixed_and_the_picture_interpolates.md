Type: bug
State: open
Parent: 1953
Depends: 1957
Area: sim

# The simulation steps fixed and the picture interpolates

**Benchmark** — Unreal: variable step with physics substeps. RAGE: **fixed step, replay- and network-exact**, interpolated to the display. **Taking RAGE** — determinism is a mechanism, and nothing else delivers it.

**MEASURED FIRST, AND HALF OF THIS ALREADY STANDS.** `Engine::Advance(elapsedS)` carries an
accumulator (`OwedS`), a fixed `Motion.StepS`, an arrears cap (`MostStepsInArrears`) and a discard
when the cap is exceeded, and `Advance()` with no argument is one fixed step and nothing else. The
simulation half of this item is built.

**The picture half is absent entirely.** Nothing interpolates: the frame shows the last state that
was simulated, so the moment the step and the frame disagree the motion stutters. The renderer does
carry a previous state -- `PrevAnchor`, `PrevMvp16` -- but per INSTANCE it is a constant
(`GltfStudio.cpp:411` writes the studio anchor into it), so there is no earlier placement to
interpolate FROM.

That is why this now depends on board:1957: a previous placement per instance is exactly what a
scene proxy fed by deltas holds, and building it twice would be the second spelling this tree keeps
producing.

CLAUDE.md calls the engine "temporally DETERMINISTIC" in its own definition of what an engine IS,
and nothing in the tree delivers it. A wish in a definition is not a mechanism.

**RAGE's answer, and it is why replays and network sync work there**: the simulation advances in a
FIXED step, in one fixed order, and the display INTERPOLATES between the two nearest states. The
frame rate then has no influence on what happens -- only on how smoothly it is shown. Unreal takes
a variable step and substeps physics underneath, which is why Unreal is the weaker row here.

Consequences the tree must accept: an accumulator, a step that may run zero or several times per
frame, a bounded catch-up so a stall cannot spiral, and rendering that reads two states rather than
one.

- [x] the simulation advances only in fixed steps: accumulator, fixed step, arrears cap
- [ ] the frame rate does not enter any result -- the arrears overflow DISCARDS time, so a stall
      changes the trajectory of a wall-clock-driven run
- [ ] the same scenario at two frame rates produces the same trajectory to the digit, proven by a case
- [ ] the picture interpolates and shows no stutter when the step and the frame disagree
