# Facade elevation scopes: bounded SDK observation

Owner: #574, parent #32. This is an operator reference, not a recovered Esri
architecture recipe or an equivalence claim.

## Primary source and visual reading

Müller et al., *Procedural Modeling of Buildings*, SIGGRAPH 2006,
[local PDF](../siggraph/2006-procedural-modeling-of-buildings.pdf), page3/Figure4,
was rasterised and optically inspected. Ground/upper floors and repeated facade
components illustrate nested scopes with absolute/relative subdivision.
Our three vertical zones and window proportions are our own bounded experiment.
The paper does not prescribe these proportions or identify a real source facade.

## Local binary observation

SDK3.4.12206, Linux core `libcom.esri.prt.core.so`, SHA256
`850a1b10182e9793f67791438989835de08a9facc2bd0d4f12c26c9a8be5ab45`.
Ghidra12.1.2 headless, Java21, x86 LE64, load base `0x100000`; run terminal0.
The independently selected core functions are:

| Function | Ghidra address | Recovered instructions | Direct named observations |
|---|---|---:|---|
| `GC::SSplitNode::prepareSplit(Processor&)` | `004d2470` |110| `pushNewFrame`, `invokeStatic`, `addChildScopeAndMakeCurrent`, `checkFloat`, `pushMethodArgs` |
| `GC::SSplitNode::split(int)` | `004d6120` |761| `isFloating`, shared-owner release |

The preparation decompilation shows child-scope tracking and a floating argument
check. The split decompilation also contains indirect calls. The nominal Ghidra
body is454/214bytes; recovered control-flow coverage is454/3552bytes against ELF
symbols472/3565bytes. This is not proof of complete type, branch, arithmetic or
floating-split semantics. Two/thirteen indirect sites remain unresolved.
No complete Esri grammar, recipe, modeller source or rendered output was obtained.
The observation supports separating split preparation/scopes from rendering;
it does not validate our metrical window or facade choices.

## Our implementation boundary

`elevation_plan` chooses a common base/body/top partition before view selection.
`elevation_cut` applies the existing open-surface CGA slab cutter to actual wall
triangles. Cutting all wall elevations at the common boundaries avoids unmatched
corner edges. An explicit zero-origin binary scale preserves original source
doubles through shape emission; tight decimal normalisation had changed some cap
vertices by one ULP and failed the independent manifold check.

The old entrance/shop/bay/balcony owner remains on its original elevation. Other
free elevations may use distinct scope-local repeat UVs and actual near recesses.
No public modelling, provider, reservation or native API changes. Wide ordinary
sources only; explicit short/low/supplied-window/unsupported-roof fallbacks remain.

Private originals: `/tmp/outshine-buildings-elevation-esri-9e83be207/ghidra-core.json`,
two `reference-004d*.c` exports and `/tmp/outshine-buildings-elevation-ghidra.log`.
They are local inspection evidence, not bundled implementation or an SDK oracle.
