# Native plant recipes — #1 / #48

`PlantRecipe` ports the existing Python `PlantPlan` circular-axis recipe semantics.
It creates canonical immutable axis/leaf rows from level and blade parameters;
no Python-generated row tables are required by the native generator. The existing
[PlantPlan owner](native_plant_plan.md) and
[prepared product assembly/cache](prepared_plant_assets.md) remain its consumers.
This is a component integration, not a replacement of world tree cards yet.

## Demand and ownership

A demand names a seven-slot canonical branch path and maximum axis/leaf rows.
The full recipe has at most eight orders,64 children per order and64 blade
variants. Preflight visits the bounded levels/slots, computes checked source
prefixes and counts, and allocates nothing. Excessive budgets, invalid parameters,
unknown paths, noncanonical padding and overflowing counts fail before growth.

A selected branch generates its descendants and the necessary ancestor chain.
Only descendants generate leaves. Sibling axes and ancestor/sibling leaves are
never built. Canonical breadth-first IDs, original global leaf offsets, parent
ownership, paths and blade variants survive subset selection. Thus a partial
request gives the same selected states as a full request, without first building
and discarding the full plant. The resulting plan owns its vectors and indices;
query spans borrow from that owner. Its existing indexing costs still apply.

Supported recipe controls follow the reference: taper/rounded length profiles,
branch whorls, attachment jitter, inclination interpolation, upward bias,
circular bends, spiral/distichous/opposite/decussate/whorled leaf placement,
sky alignment, petioles and bladeless wood. Whorled leaves retain the reference's
three members; branch whorl size is independent. Native records have explicit
metre/radian fields; [canonical JSON input](native_plant_recipe_json.md) is a separate boundary.
Per-axis/leaf randomness uses [the compatible recipe stream](../../../doc/references/procedural/pcg-compatibility.md).

## Verification

```sh
JOBS=6 make suite SUITE=outshine/src/generators/flora/PlantRecipe
```

The two cases run normally, with ASan/UBSan and container hardening:6/6PASS.
They cover exact preflight counts, global identity under branch selection,
bladeless plans, allocation-free preflight and insufficient-budget refusal,
unknown/padded paths, parent-radius constraints and unrepresentable leaf counts.
They link only seven flora units; no renderer, provider, world or asset database.
Product units and the independent consumer pass focused clang-tidy with full build
flags. Case analysis remains red at the existing16 shared Check.h findings and
three libc++ parameter-name diagnoses for legal global new/delete replacements
(#603); no suppression or complete lint pass is claimed.

`NativePlantRecipeReference.cpp` is an independent test consumer. It reads a
recipe/demand, calls native preflight and generation, and returns actual rows.
`native_plant_recipe_reference.py --binary <consumer> --output <system-temp>`
compares against the Python library; use `PYTHONPATH=lib/src` when not installed.
It reads the existing fixed recipe without rewriting tables, hashes or oracles.
The35-case finite cohort covers the fixed full plant and three subsets, all five
leaf arrangements with both length profiles and extended whorl/jitter/inclination
controls, high64-bit seeds, and bare wood. IDs, ownership, paths, variants and
counts must agree exactly. Float64 comparisons use an absolute2e-14 tolerance;
receipts report position/length/radius errors in metres, bends in radians and
frame/parameter errors separately. This finite roundoff comparison is not a
surface-error bound, LOD allowance or screen-space measurement.

## Actual cache/Metal component path

A private probe now reads only the fixed recipe and demand, generates the native
plan, assembles the existing PreparedPlant, and resolves it through the shared
AssetCache. A fresh hit process receives empty stdin and calls no generator.
Both640×720Metal PNGs are pixel-identical to the previous canonical-row portrait:
SHA256 `bc27866ac006a2ce934804c47381fb33c6de64a58d2b5b1ca8aa7b3baaaba279`.
The image was opened: a thin young tree on black, with no ground or mature crown.
This proves the recipe path preserves the technical portrait, not a visual upgrade.

The same product holds15600unique vertices,27680unique triangles,571leaf
placements and806214encoded bytes. One local diagnostic measured native plan
creation0.125ms, assembly0.626ms, hit resolve0.921ms and decode0.148ms.
These are individual component timings, not p99, SSD throughput or a device/world
budget. Original input, commands, source/binary hashes, logs and images remain
privately under `/tmp/outshine-native-plant-recipe-39bf9e07d/`.

## Remaining integration

Recipes/plan persistence and catalogue lookup must use the shared asset request
path. World selection must choose verified species and budgeted near/far products,
with stable transitions, wind, material/light response and Metal/Vulkan images.
The fixed1.55-metre young reference is a technical probe, not a mature-tree recipe
or a complete forest. No scaling of this probe proves mature vegetation quality.
