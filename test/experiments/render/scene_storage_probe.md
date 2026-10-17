# Native scene ownership probe (#15, #634)

This diagnostic compiles a private, instrumented `RuntimeScene.cpp`. The product sources,
headers, shaders, archive and build rules stay unchanged. The extra object precedes
`build/liboutshine.a` in the regular client link, so its RuntimeScene definitions satisfy
the references before the archive's original object is considered. Use a normal static
archive build without LTO or whole-archive linking. No validation layer or profiler is enabled.

## Build and run

Build the target checkout normally and generate its current `compile_commands.json`
with `make db`. Use the same compiler environment and libraries as that build.
Pass an explicit output under the system temporary directory; on this Linux host use `/tmp`.

```sh
python3 test/experiments/render/scene_storage_probe.py \
  --checkout "$PWD" --output /tmp/outshine-owner-probe-current
```

This produces the private translation unit, object, compile log and `build.json` with
commit, dirty state, input hashes, exact arguments and terminal exit status. The compile
database must belong to that checkout. Missing/ambiguous instrumentation anchors fail
before compilation instead of silently dropping a phase.

Relink the regular native client with `RuntimeScene.owners.o` immediately before the
archive and change only its output to the private directory. For automatic relinking,
save the exact regular client compiler/linker invocation as a JSON array of arguments,
then add `--link-command /tmp/outshine-client-link.json` to the command above. Relative
paths are resolved by running the command in the target checkout. The runner inserts
the object and replaces `-o`; it records the actual resulting link and binary hash.
It deliberately consumes the existing link instead of maintaining a second platform
dependency list. Do not pass a shell command, another checkout's sources, or an already
instrumented link. The manifest fingerprints the archive but cannot certify that an
arbitrary preexisting archive was built from HEAD; retain the normal build's evidence.

Run the resulting client with the ordinary scenario/measurement invocation, identical
to its control, capturing stdout and stderr outside the repository. The diagnostic adds
no product CLI option or Place-dependent code. `shots` on the current native candidate
is transitional; the generic application benchmark migration remains owned by #596.
Compare image hashes/pixels and open the images. A full native build or frame-budget
pass is not implied by successful compilation of this one translation unit.

## TSV schema, version 1

All counts are decimal; byte counts use bytes, not MiB. Pointers are process-local identity
evidence and cannot be compared across runs. Reporting occurs only at four lifecycle
boundaries: `geometry-held`, `geometry-packed`, `geometry-cooked`, `scene-destroy`.
Products whose summed reported owners hold less than 1 MiB are skipped. There is no
per-frame traversal or printing. Diagnostic overhead is unmeasured, so these runs are
ownership/image evidence, not an uninstrumented frame gate.

| Tag | Fields after tag |
|---|---|
| `OWNER` | phase, RuntimeScene pointer, `held`, Geometry.storageBytes, `driven`, Geometry.storageBytes, `shape`, owned ShapeStore vector-capacity bytes |
| `ARRAY` | phase, RuntimeScene pointer, stream, used bytes, capacity bytes, data pointer |
| `PART` | RuntimeScene pointer, part index, position floats, index elements, normal floats, tangent floats, UV0 floats, UV1 floats, colour floats |
| `PLACEMENT` | RuntimeScene pointer, part index, identity flag, 16 placement-matrix components |

`PART` and `PLACEMENT` appear only at `geometry-held`. The 13 `ARRAY` streams are
parts, positions, normals, tangents, uv, uv1, colours, indices, surfaces, lamps, clusters,
spheres. Their capacities sum exactly to `OWNER.shape`. Matrix element order is the
native `Mat4::operator[]` order; values use 17 significant digits. `PART` counts feed
the existing `shape_capacity.py` allocation model directly.

The original Geometry and its renderer representation are distinct owners. Shared
attribute spans own no bytes; a reduced ShapeStore total says nothing about their
retaining owner's lifetime. Geometry storage, nested material/image payloads, allocator
overhead and GPU restore sources may have different accounting boundaries. Do not add
overlapping world/region/building counters. This probe excludes global heap/RSS,
driver allocations, physical VRAM, GPU timing and async last-reader correctness.
World replacement and aborted candidates need separate lifetime/peak tests; #634 owns
the product contract. No raw attribute span may outlive its immutable owner.
