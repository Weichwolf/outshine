# Original road depth coverage

Owner: [#915](https://github.com/Weichwolf/outshine/issues/915).
The functional road asset remains unchanged. This diagnostic measures selected road
primitives against the actual complete scene depth in one fixed native view. It is
an oracle for evaluating future demand/LOD selection, not a streaming algorithm.

## Measured scope

The existing selected-native fixture contains 830,208 road primitives in 6,486 jobs.
All consumed original road indices, position words and model transforms match the
new capture. The new small fixture adds the complete current 1280×720 D32 image and
actual PBR raster view. Capture observers reproduce the unchanged final PNG exactly.

| Replay | Covered pixels | Primitives represented |
|---|---:|---:|
| Roads against road self-depth | 148,508 | 17,319 |
| Roads against complete scene depth | 18,427 | 1,024 |

`18427 / (1280 * 720) = 1.9995%` of pixels pass the full-scene gate.
`1024 / 830208 = 0.1233%` of selected road primitives have a surviving pixel.
There are 202 original jobs represented in those pixels. This does not say how many
coarser primitives suffice, or count hardware invocations, shadows or future views.
Exact depth ties conservatively admit surfaces; they do not identify the owner of
final color when different surfaces have identical depth.

The first replay used the stored CullView and had zero exact depth matches. Its
matrix differed from the current raster view at six Float32 components. Cull data
can be reused across frames; it is not automatically the current PBR raster state.
Using the directly observed current GeometryRenderer frame view gives 18,427 exact
D32 matches, with no road replay sample in front of the original scene. The failed
control remains in the private capture receipts; no tolerance was widened.

## Reproduce

Build a native product archive and its actual compilation database, then run:

```sh
python3 -B test/experiments/render/native_road_coverage.py \
  --checkout "$NATIVE_CHECKOUT" \
  --output "${TMPDIR:-/tmp}/outshine-road-coverage-example"
```

Use a fresh output path. The fixed depth capture requires its recorded device,
Apple A18 Pro. Another backend needs its own original scene-depth/raster capture;
passing position shaders on another GPU alone does not validate this depth oracle.
The runner hashes inputs, compiles with the actual native CDB and canonical link
flags, and exercises original, all-zero and all-one scene depth. Empty depth must
reproduce road self-depth identities; fully blocking depth must admit no road pixel.
Every surviving sample in the actual capture must equal the original D32 word.

Vertex identities avoid requiring fragment PrimitiveID/Geometry shader features.
The diagnostic expands triangle corners to 39,849,984 bytes, preserving each input
Float32 word. Its IDs/depth outputs are byte-identical to the initial Metal
PrimitiveID replay. This expansion is not proposed production geometry or a cost
optimization. Temporary geometry copies are removed after successful verification;
compressed readbacks, commands and terminal receipts remain in system temp.

## Remaining product work

The oracle reads full scene depth and fine geometry which a real demand selector
must avoid loading. Occluder acquisition, visibility maintenance, IO, decoding,
residency and uploading must be paid in a complete native comparison.
#10/#12/#915 require small coarse all-direction metadata/qualified coverage before
independently compressed spatial road-purpose/LOD pages. Precise contacts and
simulation use separate demand. AABB, unknown coverage or one fixed camera cannot
certify an omitted subtree. Moving cameras, disocclusion and shadow/reflection
needs remain separate tests; no p99 or whole-world admission follows here.
