Type: bug
State: open
Parent: 1953
Area: sim

# The simulation steps fixed and the picture interpolates

CLAUDE.md calls the engine "temporally DETERMINISTIC" in its own definition of what an engine IS,
and nothing in the tree delivers it. A wish in a definition is not a mechanism.

**RAGE's answer, and it is why replays and network sync work there**: the simulation advances in a
FIXED step, in one fixed order, and the display INTERPOLATES between the two nearest states. The
frame rate then has no influence on what happens -- only on how smoothly it is shown. Unreal takes
a variable step and substeps physics underneath, which is why Unreal is the weaker row here.

Consequences the tree must accept: an accumulator, a step that may run zero or several times per
frame, a bounded catch-up so a stall cannot spiral, and rendering that reads two states rather than
one.

- [ ] the simulation advances only in fixed steps and the frame rate does not enter any result
- [ ] the same scenario at two frame rates produces the same trajectory to the digit, proven by a case
- [ ] the picture interpolates and shows no stutter when the step and the frame disagree
