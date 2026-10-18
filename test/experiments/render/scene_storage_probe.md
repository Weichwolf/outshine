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

## Optional complete world overlap

Add `--world-overlap` when building the private object/client to prepare and abort one
complete `All` candidate before the first draw whose original geometry holds at least 1 MiB.
The probe runs before `PublishedWorldScope` and rejects an already active world scope.
It uses the existing bounded `OfflineUploads(5s)` protocol and drains GPU work before the
snapshot and after abort. Every copied piece, material/classification and terrain resource
is restored by the normal world-replacement API; content and view remain unchanged.

This is a synchronized ownership/overlap experiment. It deliberately adds work and waits
to the first draw and cannot pass a frame-time or normal streaming gate. Use a separate
uninstrumented client for device/frame acceptance. Compare/open the final PNG against
that client, check complete frames and record process peaks separately. A failed candidate,
missing rows or changed image invalidates this comparison. The product sources stay unchanged.

`HEAP_SOURCE` reports `cpp_instrumented`, then its boolean validity. `OVERLAP` reports
`before`, requested live C++ heap bytes, `during`, requested live C++ heap bytes; a second
row reports `after-abort`, requested live C++ heap bytes. These values are unmeasured when
`cpp_instrumented` is false; they exclude driver/physical VRAM and are not an allocation count.
`SOURCE`/`OWNER` add phases `world-before` and `world-overlap`, the latter for both scenes.
Deduplicate shared original products by Geometry pointer before summing reported capacities.

## TSV schema, version 2

All counts are decimal; byte counts use bytes, not MiB. Pointers are process-local identity
evidence and cannot be compared across runs. Default reporting occurs only at four lifecycle
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
| `SOURCE` | phase, RuntimeScene pointer, role (`original`/`attributes`), Geometry pointer, Geometry.storageBytes |

`PART` and `PLACEMENT` appear only at `geometry-held`. The 12 `ARRAY` streams are
parts, positions, normals, tangents, uv, uv1, colours, indices, surfaces, lamps, clusters,
spheres. Their capacities sum exactly to `OWNER.shape`. Matrix element order is the
native `Mat4::operator[]` order; values use 17 significant digits. `PART` counts feed
the existing `shape_capacity.py` allocation model directly.

`SOURCE` records the original geometry and, when supported by the target checkout,
the immutable source retained by the attribute views. Equal Geometry pointers denote
the same product: count its storage once across roles/scenes, not once per lease.
Different pointers at `geometry-held` can expose an older product still retained by
the previous shape. The template also compiles against older stores without a lease.
Control-block/allocator overhead is not included in Geometry.storageBytes.

The original Geometry and its renderer representation are distinct owners. Shared
attribute spans own no bytes; a reduced ShapeStore total says nothing about their
retaining owner's lifetime. Geometry storage, nested material/image payloads, allocator
overhead and GPU restore sources may have different accounting boundaries. Do not add
overlapping world/region/building counters. Default reporting excludes global heap/RSS,
driver allocations, physical VRAM, GPU timing and async last-reader correctness. The optional
world-overlap mode adds the instrumented C++ heap snapshots described above; process RSS
requires a separate recorder. Async last-reader correctness needs separate lifetime tests;
#634 owns the product contract. No raw attribute span may outlive its immutable owner.
