# Native terrain fragment inputs

Fixed A18 Pro/Metal inputs for [#1684](https://github.com/Weichwolf/outshine/issues/1684).
Each scene contains 64 spatially distributed samples from frame 59 of a complete,
fresh offline 720p60 / 60-frame 360° benchmark on product commit `f24ae481f`.
These are diagnostic shader observations, not uninstrumented output oracles.

`record_layout` describes consecutive four-lane groups. Each point has 52 float32
observations followed by `which`, `runner_up`, palette FNV32 and shader ID.
JSON numbers retain the original binary32 values. `palette_words_u32` preserves
all 319 bound words bit-for-bit, including integer headers encoded as float bits.
Decode numeric palette values with `struct.unpack('<f', struct.pack('<I', word))`.
The palette has 14 materials; its unmapped row is 13. A negative material selection
remains negative; use the existing shader fallback rules rather than clamping it.

`world_m`, `world_dx` and `world_dy` are actual fragment variables before material
composition. `surface_normal_and_facing` contains the unchanged, unnormalized
interpolated fine normal and front/back sign. Base/final shading normals are unit
vectors. Material weights, footprint/edge values, composed appearance and derivatives
are captured in the same invocation. Positions are scene-local shader coordinates;
`world`, `camera`, final pose and source hashes bind their frame and phase.

Every selected fragment has exact binary32 depth equality with the same command's
final opaque Depth32Float image; ambiguous pixels are excluded. Transparent/water
and atmosphere contributions can still appear later. All palette words are copied
on the GPU; every record's palette hash agrees. Exactly one native timing sample
matches the capture command, at frame 59, within 0.500001 ns printing tolerance.

The private capture adds early fragment tests, bounded GPU stores and asynchronous
depth readback. Its images differ from normal rendering; exact pixel statistics and
both shader hashes are embedded. This fixture proves no output equivalence, frame
cost, global quality or acceptance tolerance. The normal control images are bytewise
identical to the previously reviewed Places and were opened alongside the captures.
Producer/validation decisions and evidence belong to #1684; no product code changed.
