# Spatial plant in the native world — #1 / #23

This diagnostic replaces one existing nearby tree card with cooked C1 wood and
original spatial foliage. It uses the actual offline world, native Metal renderer,
unchanged camera and all other original placements. It does not register a new
production generator or publish a replacement Place/gallery image.

## Reproduction

Use a Python environment with the dependencies from `lib/pyproject.toml` and the
vegetation experiments. The native checkout needs built archives, shaders and its
compilation database; the current checkout needs its compilation database. The
driver uses complete compilation flags, compiles current plant units explicitly
and links them ahead of the native archives. It leaves all outputs in system temp.

```sh
PYTHONPATH=lib/src python3 test/experiments/render/native_plant_world.py \
  --native-root "$NATIVE_CHECKOUT" --output /tmp/outshine-spatial-plant-run \
  --place Koerbersee --bearing-deg 240 --pitch-deg -0.5 --blade-segments 4
```

The output directory must be new. `--inputs` selects another compatible versioned
cold source; the default is `test/experiments/vegetation/data/crown682` from #704.
Its six manifest digests are checked before export. The original 206-node seed
prefix selects path continuations; every original wood edge is owned exactly once.
The normalized crown input retains nodes, variants and physical recipe, not the
historical growth diagnostics. It is not a new growth simulation.

Preparation writes crown, prepared wood coefficients and eight native blade meshes
as three slices of one existing AssetCache package. The world consumer receives
only that database/request identity: its three generator callbacks are empty.
An existing package can instead be supplied with `--assets` and `--request`.
`--world` can reuse a diagnostic placement snapshot; otherwise the driver obtains
one itself before either measured comparison. Snapshot acquisition is separate.

The old selection reader checks original placement bits; the nearest forward root
is selected once. One old card is removed, the remaining 4083 placements remain.
Source axes are converted from Z-up to Y-up and scaled to that existing tree's
height. Source IDs, variants and poses remain unchanged. Capacity is checked before
pose expansion or wood meshing. This baseline intentionally requests all hosts of
one selected near tree; it does not implement projected per-host LOD selection.

## Observed world comparison

Native checkout `febe144f8`, 1280×720/30, exactly30 frames and360°; only the last
frame is saved. All compared outputs were opened. A fully automatic cold-source
replay reproduced both hashes exactly, including the spatial result previously
obtained from a private source cache:

| Representation | PNG SHA-256 prefix | All-view model triangle references |
|---|---|---:|
| Existing flat card | `076343f482591dc7` |16 for the selected tree |
| Native blade8 intervals | `676f767271160c3a` |3,255,888 |
| Native blade4 intervals | `6126a1057b4a42d2` |1,815,888 |
| Native blade2 intervals | `7f96d3e3c462dff2` |1,095,888 |

Each spatial candidate includes195,888 wood triangles and90,000 instances of eight
shared leaf meshes. These counts describe submitted references, not visible or
uniquely stored triangles. The world `ROW` triangle counter covers buildings only;
the separate `PLANT` line records these additional references and model bytes.

Summing actual leaf triangle areas at every original variant count gives411.887m²
for8 intervals,403.458m² for4 and286.122m² for2 before world scaling. Therefore
the2-interval mesh loses30.53% of reference area and is rejected. Four intervals
lose2.05%; this is a comparison candidate, not a proved LOD/error/transition bound.

The automatic4-interval package contains1,091,222 payload bytes, including820,132
bytes of prepared wood. Its world process measured6.636ms package load/decode and
27.292ms detail construction/registration. Existing whole-world warm preload was
989.541ms for the flat control and996.595ms for the spatial comparison. These are
individual host observations, not A18-Pro/device guarantees.

The same pair's30 GPU timestamps sum to516.911ms and567.666ms: means are respectively
`516.911/30 = 17.230ms` and `567.666/30 = 18.922ms`. Maximums are37.721ms and42.032ms,
both above `1000/30 = 33.333ms`. Other individual pairs fluctuate; no stable timing
ranking or passed device gate follows. Complete raw rows, commands, heads and
output hashes remain in the generated `receipt.json`.

## Decision and remaining integration

Spatial foliage and continuous wood visibly improve the near silhouette and
depth over the old flat card. This full-reference near path is still too costly:
`(90000 + 1) * sizeof(Mat4) = 11,520,128B` of CPU model matrices for one root.
The blade interval reduction leaves that cost unchanged. All-source expansion is
the reference control, not the proposed forest runtime algorithm.

Next use the verified shared leaf carriers from #704/#707 and projected host
selection from #716. Preserve physical coverage, source ownership, material
identity and coherent transitions. Their premultiplied object-frame normal moments
are not tangent-space normal maps and require an explicit compatible consumer.
The present baseline uses native default thin-sheet reflection/transmission, not
the delivered procedural pigment/normal/variance fields. Production registration,
bounded residency, cheaper instance representation, bark/joints, wind, complete
near/far/shadow transitions and all-Place/device acceptance remain open.
