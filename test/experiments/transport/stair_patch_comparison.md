# Exact local terrain patches across the five-module Wiener history (#542)

[#539](stair_prefix_comparison.md) leaves a verified 53-source network whose
fifth module cannot be loaded through the intentionally bounded four-parent
diagnostic importer. This experiment represents the already verified root
surface and all five exact local field changes independently. The legacy
importer, source/owner/solver limits and existing generated bodies stay unchanged.

Reusable API and exact finite geometry contract:
[`geometry.surface_patches`](../../../lib/surface_patches.md).
`stair_patch_experiment.py` converts explicit verified source products to
prepared resident patches; `stair_patch_verify.py` reads them without provider,
CDT, solver or global field generation. `stair_patch_cost.py` compares indexed
lookup and complete scans, and a flat equal-surface control. This is an
experiment format, not a second AssetCache or native runtime codec.

Stable patch/local-face IDs replace global facet indices only in this new
geometry view. Every removed face must be active, every whole XYZ boundary edge
must match, all face attributes/frame/kind remain explicit. Old meshes are shared
unchanged; no numerical welding occurs at a patch seam. Failed changes preserve
the old valid view. Source/level/access/reservation semantics remain the caller's
contract, storage/index/residency #10 and world hierarchy/LOD #12.

Initial exact replay: all five updates reproduce the same final 27962 XYZ
facets and ground/constructed tags as the saved 53-source reference, with
106/55/131/200/48 exact boundary edges. This establishes finite representation
equivalence, not new source coverage, network generation or native acceptance.
The next generator adapter still needs to consume local view products.

SIGGRAPH starting point: Lindstrom et al. (1996), page 4 figure 3 inspected
visually. Its local height-field/delta representation motivates inspecting
explicit edges; this operation is independently defined and implements none of
its LOD/morphing/simplification method. No SDK or external code is incorporated.

Frozen preparation/replay, actual local contact checks and 3D images, installed
controls and scoped cost results follow after complete terminal acceptance.
Whole-world asset streaming/eviction/LOD, further source/level/profile/route
coverage, reservations, statics and native integration remain #434/#50/#48.
