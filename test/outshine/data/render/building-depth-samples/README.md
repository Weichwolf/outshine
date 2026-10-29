# Native stored-depth sample comparison

Actual A18/Metal readbacks for #1752. Complete selected original group from
`../building-full-view/`: cells25/33/34,1558sources,38476original triangles.
Previously unresolved cell41 remains outside this explicit control.
The existing128² first-hit field produces5374sample triangles.

`manifest.json` binds product commit, driver, original geometry, capture and queries.
`captures.npz` contains four independent original/sample pairs. Array names identify
case, representation and buffer. Reconstruct1280×720 from the declared clear value
and crop; the exporter checks the entire frame bitwise before discarding borders.
`linear`, `depth`, `normal` belong to the base draw; `ss16` averages sixteen actual
microcamera draws. `high-sampling/` retains one32px control with4096microcamera
draws; `same-rays/` retains the orthographic capture-ray control. Each directory
has its own manifest and lossless arrays. These finite references are not converged
ground truth or a World, motion, screen-error, performance-budget or global-quality
approval.

Representation decision and current limitations belong exclusively to #1752.
