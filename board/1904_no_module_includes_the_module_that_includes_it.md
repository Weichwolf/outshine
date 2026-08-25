Type: bug
State: open
Area: render, world
Tags: measured, layering

# No module includes the module that includes it

`--audit-layers` proves the TIER graph is acyclic. The module graph beneath it is not, and
`build/STATE` names both cycles the first time it ran:

    CYCLE render and render/stages include each other, 12 deep and 28 back
    CYCLE world/generators and world/ground include each other, 3 deep and 2 back

A cycle means neither module can be read, built or replaced without the other. `render` holding
28 includes from `render/stages` and handing back 12 is not a layering with a seam in it; it is
one module spelled in two directories.

`--audit-layers` cannot see these because it judges the first path component only, and both ends
of each cycle sit inside one tier. The declaration it walks stops where the cycles start.

## What will be true

- [ ] `LayerReaches` is declared at MODULE granularity, not tier -- `render/stages` reaches
      `render`, or `render` reaches `render/stages`, and one of them is chosen and stated
- [ ] `--audit-layers` walks that finer table and refuses a cycle by name
- [ ] both cycles are cut

Proving test: `--audit-layers` with a module table. Negative control: the table at this commit,
which admits both cycles, and the audit refusing them by name.
