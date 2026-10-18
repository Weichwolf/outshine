# Current pier inputs keep historical detour pins strict

Issue #766, reconstruction/cost follow-up #749. Python research only.

The freshly reconstructed 78-source cohort retains its verified current bodies.
Its pier 123 differs from the versioned historical obstacle by at most
2.2044922900477104e-6 m in Z. XY vertices and triangle indices are identical;
projected footprints are equal, but Float32 vertices are not all byte equal.
The numerical cause is not established. The old whole-XYZ pin must reject it.

Historical XYZ-facet SHA256:
`18d8cd1daa6ac4d4301eee8cd5b717fe6e6d61c04f9f71f440be4879580e5680`.
Current XYZ-facet SHA256:
`77ef635097217484cc59dbedcd6a04a311b86379eb2ac9a470825ba5caaec0b3`.
The old `stair-pier-detour-vienna.json` and obstacle NPZ are unchanged.

## Explicit preparation, not admission

`stair_axis_detour_inputs.py` loads the existing verified resident/module chain,
copies the explicit template and derives a new whole-facet SHA256 and actual
projected footprint for each selected current pier. Source IDs, original segment,
selection and route parameters remain. Its separate `.binding.json` records
producer commit, original template, resident, frame, support hash, parent reports,
new recipe and old/current body hashes. Neither output admits a road or world.

`stair_axis_detours.witness` shares the previous hash/projection operation with
the unchanged strict binder. A later body change still rejects the new recipe.
The usual detour, source/body, whole Double/Float32, terrain, profile and pier
checks decide actual geometry admission. There is no relaxed historical hash,
new cache manager, native/library API or SDK dependency.

Run the preparer with explicit `--resident`, the same ordered `--prior-module`
arguments as the intended local construction, `--template` and a fresh
`--output /tmp/outshine-current-detour/recipe.json`. Use that new file as the
normal `stair_local_generate.py` / `stair_local_verify.py --fixture`; follow with
`stair_local_reference.py` and actual whole-scene rendering. Preparation has
120 CPU seconds/3 GiB address-space limits; no output overwrite is allowed.
It records only verified input provenance, not a substitute saved geometry proof.

## Fresh evidence, 2026-10-18

Implementation/preparation/readers: `7b7df8a0d96e68ec426d0db497133ee1f7d14d25`.
Linux Debian 13 x86_64, i5-1135G7, Python 3.13.5 / NumPy 2.2.4,
SciPy 1.18.1, Shapely 2.2.0 / GEOS 3.14.1. Shared development load.
Source and installed outshine-reference 0.1.0 produce byte-identical recipes:
`bb78f887bcc2492fea5f5bdb8963020ed863bccb2a4bdc9c19ed6d962416c237`.
The preparer's output is byte-identical to the actual current-pier input already
used by the frozen `59b7b4784` generator and independent whole control.

Nine controls pass source and installed: real historical obstacle, tiny Z change,
unchanged template, old/new body mismatch, later XY change and invalid selections,
plus the existing original-knot-preserving detour controls. New saved-module
readers pass source and installed on disposable copies; producer receipts remain
untouched. Ruff format/check and `make lint-docs` pass. Native gates are outside
this Python change.

Actual source `39881120:32` grows the finite cohort 78→79. Independent whole
control proves 31,860 exact XYZ facets/attributes and bit-exact module/layout/
field arrays. Existing 124 whole piers remain checked. The actual complete
1280×720 Workbench image was rendered and opened. Coverage is still sparse,
contrast weak; micrometre rebinding is not a visible quality improvement.
No image or replacement gallery is published, no worldwide/native admission.

The selected 44-face witness materializes 3,168 XYZ bytes for its ordered facet
hash; projection/GEOS and full-chain loading have additional unmeasured costs.
No optimization/device-budget claim follows. This only removes an obsolete-input
blocker while preserving strict geometry checks; all #749 cost obligations remain.

Current products, original input copies, source/installed receipts, binding
countercase, images and terminal logs remain privately under
`/tmp/outshine-transport-resume-1f10d55eb/`. Reconstruction through 84/90 and later
source growth is separate work under #749; historical 102-source pins are not
requalified by equal counts or geometric similarity.
