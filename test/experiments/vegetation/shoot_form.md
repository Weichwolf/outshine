# Shoot size and lamina roll on the original crown inventory

Owner [#727](https://github.com/Weichwolf/outshine/issues/727), narrow child of
the unfinished [mature oak study](oak_scaffold.md). The original near crown
shows repeated fans. This experiment separates shoot-length variation from
lamina orientation. It preserves the original skeleton and physical leaf
inventory, with explicitly new pose/product identities. Neither a new oak
species default nor a full botanical/forest/LOD admission follows.

## Authored controls and invariants

Use the six unchanged files at `data/crown682/`, source identity
`62a49f3c918384dba7633ec6e13a3f48560c828df682c0745cea7e0e8115015d`.
The [exact Float64 compact source](compact_crown.md) supplies4301 hosts and90000
original IDs/variants. The rejected Float32 consumer is not used.

Compile one bounded random scalar per skeleton node with PCG64(seed727).
For each host, mix .65 of its parent's value with .35 of its own, centre the
host values to zero mean and divide by their maximum magnitude. This produces
correlated authored length variation, not a growth/competition simulation.
Scale `s=1+.55*v` yields .45–1.54990, mean1; original35cm shoots become
15.75–54.246cm while retaining total centreline length and original host roots.
Fixed endpoint radii remain .5/.05mm. These are study parameters, not measured
Quercus allometry.

For a demanded leaf, let C be its original shoot centre and H its host origin.
Move its centre and origin by `delta=(s-1)*(C-H)`. Petiole direction/length and
local blade shape remain unchanged. Independent variable-length circular arcs
provide the scalar control; no main skeleton position moves.

Roll `rho=.85*u_leaf+.3*u_host` about each unchanged petiole axis. For original
frame columns X/Y/Z, the new columns are `cos(rho)*X-sin(rho)*Z`, Y,
`sin(rho)*X+cos(rho)*Z`. This is a proper rigid lamina rotation; it changes the
leaf plane without moving its root or petiole direction. Stored coefficients
are complete, immutable, content-bound and independent of camera queries.
Mean/std of actual90000 rolls: -.002023/.521216rad. Null controls bypass both
changes. Original, sizes-only, roll-only and combined use the same latent values.

Original skeleton/C1 wood, IDs, owner assignment, variants, analytical blade
areas and procedural material stay fixed. Total analytical area is392.735145m²
in every case. Scaling source shoot-centre bounds while keeping the same
rotation-invariant organ reach gives conservative new host bounds, expanded
outward for Float64 roundoff. Each capacity check precedes local pose work.
Complete stored parameters occupy about1.765MB/file, including the original
compact product, added per-host scales/per-leaf rolls and new bounds. This
trusted experimental NPZ is not a proposed native/cache format.

## Sources and independent numerical controls

Local SIGGRAPH1988 *Developmental Models of Plant Architecture*, PDFpage7,
sections4.5/5/Figure18, was optically inspected: variation can preserve overall
form and phyllotaxis is a separate spacing/angle control. This experiment does
not implement its L-system or claim a botanical distribution from it. The
earlier SIGGRAPH1995 Weber/Penn leaf-orientation page remains a useful distinction
between leaf orientation and branch structure.

SpeedTree's supplied Oak_Hero_Field preview was opened again: broad irregular
subcrowns, rich branch surfaces and gaps remain far beyond this source. It is
a Modeler quality reference, not an independently verified living-tree record.
No vendor texture/mesh/code enters generated products. Local botanical reference
supplementation remains necessary before species admission.

Actual Ghidra12.1.2 read-only/no-analysis run completes both requested targets:
CLeafNode::ComputeFrame0x1b396d0(size227hex) and
ComputeSkySensitiveLeafOrientation0x1b2e780(size1a46hex). The latter contains
normalization, axis/angle rotations, left composition and orthonormalization.
That supports inspecting lamina orientation as its own stage; private field
bindings and numeric parameter semantics remain unresolved. The authored random
roll formula above is our own, not a recovered SpeedTree equation.
Decompilation SHA256s respectively
`32b78f7b86e06cbb51e1fca763e8ab81ee92fb012afc523b453fc06c4b265107` and
`d3bfd4cb509adad7f22f8c0046672b802c82421a8ce21fc5d91cf12b964e6a29`.
All settings/cache/logs stay under/tmp; proprietary exports stay private.

Producer ca1aa954e checks3330000 transformed source/coarse/carrier vertices per
case,13320000 total. Largest signed violations are negative: -.0453561m
original, -.0453122m sizes, -.0442507m roll, -.0441555m combined. These are
finite actual-source controls plus the analytic reach derivation, not a global
floating-point certificate. Maximum independent SciPy rotation component error
6.67e-16; maximum petiole-root error3.67e-15m, within declared1e-12 tolerance.
Frame handedness/unit/orthogonality and unchanged petiole-axis bits are checked.
All90000 null ID/variant/origin/frame/anchor rows are bit-identical to the
independent original `CrownShoots.group` stream. Replay is checked separately.

| Descriptive base-axis statistic | Original / sizes | Roll / combined |
|---|---:|---:|
| Mean unsigned inclination, degrees |42.8674|48.3346|
| Inclination standard deviation, degrees |22.0163|22.2393|
| Mean host normal concentration |.820799|.727467|

These statistics show a changed orientation distribution. They are not a BSDF
filter, measured botanical distribution or evidence of better appearance alone.
Full90000-row evaluation takes .085/.128/.106/.113s on the initial shared-host
run; complete preparation including the unchanged C1 wood takes5.264s.
Process peak159137792B includes all comparisons, not just the parameter product.
Separate query/memory traces and fresh replay retain their own timing scopes.

Later three-repetition query costs, after the independent full null check:

| Form | Whole/near median s | Whole/near traced peak B |
|---|---:|---:|
| Original |.1250/.0734|13862052/8470553|
| Sizes |.0867/.0502|18158699/10223819|
| Roll |.1168/.0625|26079179/14664305|
| Combined |.1580/.0786|28239275/15883022|

Queries return anchors as well: whole final buffers11610000B. Added shifts and
rolled frame copies raise transient memory. Traced peaks/current are not
cumulative allocations, complete memory traffic or device costs. This separate
numerical process peaks175267840B. All timings are normal-priority shared-host
measurements, not a ranking among controls with different arithmetic.

## Fair detail and actual image evidence

All views use2560×1440,32 EEVEE samples and the same original procedural
pigment/height study material. This is not calibrated oak venation or optics.
Same sun/fill and camera; all light shadows disabled, so camera-only rejection
does not drop required shadow casters. Whole source C1 body195888 triangles
stays fixed; demanded short-shoot tubes follow the actual changed lengths.
Caps/side joins and generic smooth wood remain known defects.

The first near reader assigned32×4 geometry to far background leaves as well.
It was deliberately stopped without a PNG, terminal143. Fine-prototype AABBs
retain46045 leaves:46045×520=23943400 leaf triangles if all are fine.
The corrected diagnostic determines detail before expansion: each leaf ID gets
the **same** mesh across all four forms, based on their union of visible fine
prototype AABBs with projected bound≥96px. Only1676 IDs require fine geometry;
the rest use the original8×1 source mesh. This counterfactual union is a fair
comparison policy, not a runtime requirement to evaluate four plants or a
certified LOD transition. Fine prototype boxes also contain the coarse vertices.

Consequently complete local faceted inventory is identical across forms within
each view:411.886847m² whole and411.516912m² near. These values differ from the
analytical area and from clipped visible triangle area; no area gain is used to
improve density. Original near draws46045 leaves/2368402 leaf triangles,1652
fine,268357768B expanded diagnostic leaf arrays. That expanded representation
is not proposed runtime instance storage.

All eight actual2560×1440 PNGs were opened: four forms in the same whole view,
and the same four in the same near view. Sizes change spacing but leave the
repeated fan structure conspicuous. Roll varies the surface planes and their
illumination; radial placement still reads as a fan. Combined near foliage
looks somewhat less coplanar and more irregular, without repairing the basic
shoot arrangement. In all whole views the crown stays too regular/elongated
relative to the Modeler reference; smooth generic wood and joins remain poor.

Verdict: admit these mathematically checked controls as authored ingredients,
**reject them as a sufficient repeated-fan or mature-oak remedy**. No species
default changes. Further useful image work needs the actual leafy-growth-unit
placement/count/shape and distinct subcrown/branch architecture, rather than
another increase in random roll alone. Better botanical lamina/venation and
wood surfaces also remain necessary. This is the narrow experimental conclusion;
#682/#25/#1 and the overall goal remain unfinished.

The8×1/32×4 diagnostic retains the same per-ID local prototype within a view.
Whole leaf triangles3060000 in all forms,357840000B expanded leaf arrays.
Near triangles2368402–2373640 and arrays268357768–268933840B vary because new
poses change which IDs intersect the camera. This is not a runtime layout.
Across all eight actual reports, scene build15.28–42.52s, render83.01–347.67s,
Linux process RSS peak3613822976–3940364288B. Source/procedural generator time
and these software Blender costs are separate. Concurrent agent loads varied;
do not infer a control's rendering speed from that range.

Two sequential render loops terminate143 after those completed images, cause
unproven. Completed PNGs and per-frame reports are retained; five missing frames
then complete as individual commands, each terminal0. The loop statuses are not
silently relabeled as successful runs. The software backend also warns about velocity-buffer
allocation, using object/camera velocities. These static images are not motion,
memory-budget or native renderer acceptance. A64×64 small cube exit-control
completes0; attaching gdb to the long render is denied by ptrace policy, not a
successful profile. Native/device timings remain entirely unproven here.

## Reproduction

Use `.venv`, TMPDIR=/tmp, PYTHONDONTWRITEBYTECODE=1, one BLAS/OMP thread and
PYTHONPATH=lib/src:lib/tests:test/experiments/vegetation:test/experiments/render.
Run `shoot_form_probe.py --inputs test/experiments/vegetation/data/crown682
--output /tmp/<fresh-study>`. Then `shoot_form_detail.py --inputs <cold-source>
--study /tmp/<fresh-study> --output /tmp/<detail>.npz` and
`shoot_form_metrics.py --inputs <cold-source> --study /tmp/<fresh-study>
--detail /tmp/<detail>.npz --output /tmp/<metrics>.json`.

Fresh reader: `shoot_form_replay.py --study /tmp/<fresh-study>
--output /tmp/<replay>.json`. Source acquisition, plan/form build, original groups
and RNG callbacks are forbidden; all eight whole/near query hashes match.
The first fresh run decodes each full file in6.9–9.3ms, current-process VmHWM
135479296B; it expands reference CPU rows, not native GPU instances.

`render_shoot_forms.py --inputs <cold-source> --study /tmp/<fresh-study>
--detail /tmp/<detail>.npz --output /tmp/<new-render> --mode original --view near`.
Modes: original/sizes/roll/combined; views: whole/near. Set
LIBGL_ALWAYS_SOFTWARE=1, and XDG_CACHE_HOME/MESA_SHADER_CACHE_DIR under/tmp.
The measured backend is software Mesa llvmpipe, not RTX/A18 evidence. All PNGs
remain private; none are added to doc/.

Ten new controls plus the previous99 pass source and reused installed-wheel
modes,109 each. They cover zero bits, independent rotations/arcs/area, roots,
selection/refusal, immutable identities, finished replay, invalid traits and
matched detail. No lib/, native binding or production default changes. The
full image verdict is bounded as stated above; broader form quality remains
with #682. Source/renderer/proof commits and terminal receipts are linked in the
PR, with no native/device gate claim.
