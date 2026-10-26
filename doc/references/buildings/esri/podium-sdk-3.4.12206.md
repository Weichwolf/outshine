# Podium/setback reference observation, SDK3.4.12206

Building experiment [podium.md](../../../../test/experiments/buildings/podium.md)
and issue [#533](https://github.com/Weichwolf/outshine/issues/533).
This extends the existing [stepped-profile observation](stepped-profiles-sdk-3.4.12206.md).

Local binary: `~/Downloads/esri_ce_sdk-3.4.12206/bin/libcom.esri.prt.core.so`.
SHA256 `850a1b10182e9793f67791438989835de08a9facc2bd0d4f12c26c9a8be5ab45`.
Fresh bounded Ghidra12.1.2 analysis exported `SetbackOperation::setback` at
`0050bee0`,1551 recovered instructions, clamped to its ELF symbol. Headless
exit0,27.43s; global analysis disabled, indirect callees/types/body coverage
unverified. Research was performed during the dirty prototype at `ee6da7bbe`;
this is a binary observation, not frozen Python rendering evidence.

Observed calls/flow include distance setup, `calcCoreArea`, setback faces,
polygon-with-holes processing, scope mesh conversion and tagged replacement
through `MeshFaceInterpolator.replaceWithInterpolatedMesh`, followed by adding
the generated mesh to split-node output. This supports a composition model
with per-face distances, retained holes and attribute interpolation. It does
not establish exact numeric equivalence or a ready building programme.
Esri's geometric “core area” is not evidence for our functional circulation
core, room planner or terrace routes.

The preceding `op_envelope`/`op_setbackPerEdge` observations remain dispatch
observations; this deeper recovered function adds bounded geometric evidence.
Source-bound podium/wing choice, door reservations and actual physical access
are independent Outshine Python procedures and checks. No third-party binary,
decompiled source or generated export is bundled in the repository.

SIGGRAPH2003 *Instant Architecture*, page4/Figures5–6, was also inspected
optically for attributed split/component composition. The local PDF remains
under `doc/references/buildings/siggraph/2003-instant-architecture.pdf`.
