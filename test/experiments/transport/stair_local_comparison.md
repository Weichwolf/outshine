# Local generation from resident terrain and independent road bodies

Owner: [#547](https://github.com/Weichwolf/outshine/issues/547), parent
[#434](https://github.com/Weichwolf/outshine/issues/434). This delivery connects
the [exact surface patches](stair_patch_comparison.md) and the
[original endpoint correction](stair_endpoint_comparison.md) to new generation.

Prepared input keeps each old closed road body with its own source-owner
columns. The root has 32 source columns, later assets at most eight. A bounded
temporary working set has at most 16 assets, 64 local source bits and 32768
body faces. These are local capacities, not a global source mask; distant
assets do not consume local bits. The four-parent legacy importer is unchanged.

The local field initially contains whole active triangles within 40 m of the
new original axes. The generated original corridors must fit inside an exact
conservative 16 m axis buffer, leaving at least the existing 24 m soil margin.
Whole triangles intersecting any affected pier footprint complete the context;
five-metre auxiliary ground around that context preserves the constructive
soil eligibility. Explicit global prepared distance queries independently
confirm the final 24 m selection and whole-pier dependency closure. Local
terrain capacity is 8192 context / 2048 selected old faces; no partial query
silently substitutes missing neighbours. Local exhaustion retains the previous
valid world and original source catalogue with a joint trial reason.

New body construction and contact/height/clearance checks reuse the existing
profile/level_box family. Exact patch edges/coverage preserve the old field.
Whole convex source-facet references check local ground contacts in both
precisions without assuming a global Lipschitz bound on a nonconvex local
region. All old whole piers remain checked. A saved hit replays independently
verified local deltas and road assets, with ordered report hashes, without
source decode, planning, CDT, solving or whole-field/body construction.
At most 32 explicitly ordered local research deltas are loaded per run; this
is not runtime storage, paging, eviction, LOD or a general world graph.

SIGGRAPH starting point: Lindstrom et al. (1996), page 4 / figure 3 inspected
visually again. Existing exact local geometry is retained; their hierarchy,
projected errors and simplification are not implemented. No SDK or foreign
code/runtime dependency is introduced. Source/frame/topology/semantics remain
explicit, shared Plan/Reservation/Provider/native APIs remain unchanged.

## Verification scope

Unfrozen exploratory results match all module, local-plan and local-field
arrays of an independent whole-field construction exactly for the real
53→55-source extension. These are hypotheses, not frozen acceptance. Frozen
source/installed tests, fresh saved-product verification, independent complete
field control, actual 1280×720 images, work/copy/peak evidence and negative
controls are recorded after execution below.
