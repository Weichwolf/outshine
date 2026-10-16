# Reusing complete plants in flat forest consumers — #498

The #491 physical crown now reaches the existing flat forest consumer. Compare
three representations of the **same** finished wood,37464physical leaf IDs,
eight blade meshes, UVs, materials and whole-plant poses: direct leaf groups per
world root, nested complete-plant instances, and one packed leaf mesh plus the
separate unchanged wood mesh. No geometry/leaf size/density/camera/light reduction
supplies the representation comparison. The generic crown replaces both older
broadleaf demonstration categories; this does not create oak/beech species recipes.

## Product and fidelity

`stand_leaf_pack.py` preflights at most100000unique organ IDs/64blade prototypes,
proper rigid frames, valid meshes and finite UVs. An explicit default256MiB bound
covers final array bytes, **not** total allocator/scratch/process memory. A
conservative Float32 coordinate bound precedes expansion; the actual position
round trip must remain≤1µm in tree-local metres and normals≤1e-6. Batches≤4096
(default1024) reuse `biome_models.Assembly`. Per-organ ID/variant and contiguous
vertex/face runs retain correspondence; no organ is dropped or merged away.
Wood remains separate to preserve its procedural Generated material coordinates.

`stand_leaf_product.py` creates a standalone finished rendering product with
checksummed geometry/maps, source-ID provenance and a complete consumed-input
fingerprint (entry, placement tables, wood/blades and procedural material files).
Changing actual source geometry under the same old plan ID invalidates this key.
A hit validates its own metadata/payload and reads its finished arrays, without
source acquisition, growth, organ placement or packing. This is a private static
rendering product, not a new shared catalogue/cache/GameAsset API or wind product.
`stand_prototype_replay.py` separates that fresh read from full-source checking.
Its per-organ scalar matrix products independently check all physical positions,
corner normals, UVs, face indices, material indices, stable IDs and complete runs.

Actual product:1048992leaf vertices/1273776leaf triangles,351328wood triangles;
1625104triangles per whole plant. Final leaf buffers+runs106510152B; maximum
position error4.755011e-7m, normal error2.9802265e-8; source wood/UVs/faces/IDs exact.
The source is #491 plan`adcd71e051040b3284d101a824730f2c02b9513e6dce726bb765e3da870a31e0`.
Its consumed-input hash is`3b61f891decdda41cec884347e77c72265a79c1a1365a205fe5c96b9801edd0f`;
finished product`aed68442d48a0a51b3cbc5d808a569c1898846c8f1771516eaf8da4830f94bb2`.

The raw source is13825911B on disk; standalone packed product40301479B, decoding
16arrays/157979704B including unchanged wood and organ runs. This is a deliberate
storage/upload versus instance-work tradeoff. One cold build onb530ac642 finished
in4.878s wall/4.864CPU-s/622.00MiB peak RSS: source decode0.191s, packing0.832s,
fingerprint/compressed export3.199s. Forty packets produce200major Assembly part
arrays, five concatenated arrays and five Float32/index result conversions;
this is an explicit major-array ledger, not total Python/native allocations.
Fresh standalone read on6699f3820:0.695s, checks separately1.026s; entire process
2.453s/2.304CPU-s/393.41MiB peak RSS. Full-source conformance separately7.833s.

## Blender binding and matched images

An actual EEVEE render exposed a wrapper pitfall: Object Info **As Instance** on
an object whose evaluated geometry is itself instances yielded no rendered
plants. Viewport realization still reported the correct vertices, so geometry
counts alone did not detect the missing image. The local four-cube diagnostic
compared direct, object-wrapper-as-instance, forwarded geometry and a node-group
source; only the second cube disappeared. Wrapper geometry is now forwarded;
base wood/leaf meshes remain instances. Both nested and packed representations
are rendered after this correction. Failed empty images are rejected evidence,
never speed gains. Existing `instance_group` callers retain their previous default.

Source12956f747 (packed also6699f3820; renderer unchanged),1920×1080/four samples,
Blender5.2.2 EEVEE/OpenGL **llvmpipe software renderer**, shared i5-1135G7 host,
16GiB RAM, one configured llvmpipe/OMP/BLAS worker. Host load10–13; no GPU/device
budget or stable small timing advantage inferred. Two complete roots: one identity,
one0.37rad yaw/uniform0.95scale. Whole images were actually opened.

Before measurement #498 fixed channel MAE≤1/255 and95thpercentile≤3/255 against
direct, plus rejection for visible form/material/shadow loss. Nested MAE0.001246
of255/p95=0; packed0.005750/p95=0; both pass with intact crowns/shadows. Local worst
channels24/26 are disclosed; thresholds were not relaxed. After reboot the direct
reference is pixel-identical to its earlier run. `test/scripts/pixels.py` supplies
the full-image comparison; its numbers are not a botanical/optical-material gate.

| Two-root mode | Whole process s | CPU-s | Peak RSS MiB | Prototype s | Root placement s | Render s | Explicit point rows |
|---|---:|---:|---:|---:|---:|---:|---:|
| Direct | 48.953 | 57.887 | 3722.04 | 2.034 | 0.2381 | 44.559 | 74928 |
| Nested | 53.623 | 62.573 | 3722.95 | 2.358 | 0.0014 | 48.720 | 37466 |
| Packed | 57.063 | 67.552 | 3612.82 | 9.899 | 0.0017 | 44.457 | 2 |

Packed is **not faster overall for two roots**. Its larger one-time Blender mesh
binding is visible. The shared helper still converts triangle/custom-normal input
to Python lists; that offline cost remains explicit. Nested shares leaf point
rows once but still exposes leaf instances to Blender. For Nroots/Lleaves, point
rows are N×L/direct, L+N/nested, N/packed. Each point group explicitly materializes
three Float32 staging arrays (position/Euler/scale),36B per point, then writes36B
of point attributes; Euler conversion additionally returns24B per frame. These
are known buffer requests/copies, excluding selection/validation temporaries,
Blender's native allocations, meshes, driver/GPU traffic and realization work.
Whole plant triangles remain identical and do not vanish with fewer point rows.

## Reproduce

Use `.venv` and `PYTHONPATH=lib/src:lib/tests:test/experiments/vegetation` (the
installed-wheel checks omit `lib/src`). Dependencies: NumPy/SciPy/Pillow, the
installable reference library, and bpy5.2.2 for actual scene bindings/rendering.
`SOURCE` is the complete `spatial/` product from `twig_cluster_experiment.py`,
whose inputs are the saved spatial crown and #487 pose; see their documentation.
`CONTEXT` contains unchanged `grass-0..7.npz` and `pine.npz` from `biome_models.py`.
All output arguments below must name explicit local system-temp paths.

```sh
.venv/bin/python -m unittest stand_scene_checks stand_pack_checks twig_cluster_checks biome_tile_checks test_blades test_ground_cover -q
.venv/bin/python test/experiments/vegetation/stand_leaf_product.py --inputs "$SOURCE" --output "$PACKED"
.venv/bin/python test/experiments/vegetation/stand_prototype_replay.py --inputs "$PACKED" --output "$HIT_REPORT"
.venv/bin/python test/experiments/vegetation/stand_prototype_replay.py --inputs "$PACKED" --source "$SOURCE" --output "$SOURCE_REPORT"
.venv/bin/python test/experiments/vegetation/render_stand_prototype.py --inputs "$SOURCE" --output "$SHOTS" --case pair --mode direct
.venv/bin/python test/experiments/vegetation/render_stand_prototype.py --inputs "$SOURCE" --output "$SHOTS" --case pair --mode nested
.venv/bin/python test/experiments/vegetation/render_stand_prototype.py --inputs "$PACKED" --output "$SHOTS" --case pair --mode packed
.venv/bin/python test/scripts/pixels.py "$SHOTS/pair-direct.png" "$SHOTS/pair-packed.png"
.venv/bin/python test/experiments/vegetation/render_stand_prototype.py --inputs "$PACKED" --context "$CONTEXT" --output "$SHOTS" --case Laubwald --mode packed
```

Repeat the last command for Mischwald; direct/nested use`SOURCE`. Measure fresh
process wall/CPU/peak RSS separately from the receipt's decode/template/placement/
context/render stages. Keep population digests and context product hashes equal.
No server, versioned image or alternative gallery is produced.

Local primary references: SIGGRAPH1995 [Weber/Penn](../../../doc/references/vegetation/siggraph/1995-creation-and-rendering-of-realistic-trees.pdf)
(tree/group reuse) and SIGGRAPH1998 [Deussen etal.](../../../doc/references/vegetation/siggraph/1998-plant-ecosystems.pdf)
(population versus geometry, approximate instancing). Their local PDF renderings
were opened, including the1998paper's introductory pipeline/instancing discussion.
Existing local Ghidra export`CBatchedLeafNode::ComputeBatchWorker`0x1d1d5b0 was
reread: explicit cumulative seed, location-vector, mesh-index and force-alignment
operations are visible. These motivate separation of placements, prototype meshes
and local state; they do not prove our packing method is SpeedTree's algorithm
or its runtime speed. No new vendor numeric oracle, bundled decompilation/assets,
web search, native API, species/wind/collision/full forest-LOD/device admission.
