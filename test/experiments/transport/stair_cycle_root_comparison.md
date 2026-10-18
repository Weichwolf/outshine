# Complete transport root reconstructed from versioned sources

Issue: #758; follow-up: #749; network coverage and native integration: #434/#48.

## Defect and change

The full cycle recipe already computes the complete source body and terrain.
Its required historical products were used only to bind and display a previous
state. Losing private temporary products therefore prevented rebuilding from
otherwise complete, versioned inputs.

`stair_cycle_experiment.py --reconstruct` explicitly builds from those inputs.
The saved reader requires the same mode and exact fixture hash. It still checks
all source ownership, whole profiles, body topology, Double/Float32 terrain,
clearance, original DEM targets and the actual displayed arrays. Reconstruction
has one real after image; a claimed previous image is rejected. Comparison mode
still requires both matching verified previous products. No fitting, quality,
source-admission or resource threshold was relaxed; no library/native API changed.

## Reproduction

Use Python 3.13.5, NumPy 2.2.4, SciPy 1.18.1, Shapely 2.2.0/GEOS 3.14.1,
Clarabel 0.11.1 and the installed `outshine-reference` package or `lib/src`.
Set `TMPDIR=/tmp`, `PYTHONDONTWRITEBYTECODE=1`, `OPENBLAS_NUM_THREADS=1` and
`OMP_NUM_THREADS=1`. Add `test/experiments/transport` to `PYTHONPATH`.
The output directory must be fresh; substitute your explicit private path.

```sh
python3 test/experiments/transport/stair_cycle_experiment.py \
  --reconstruct \
  --fixture test/experiments/transport/fixtures/stair-north-bridges-vienna.json \
  --output /tmp/outshine-transport-root-9628f645b/products
python3 test/experiments/transport/stair_cycle_verify.py \
  --reconstruct \
  --fixture test/experiments/transport/fixtures/stair-north-bridges-vienna.json \
  --output /tmp/outshine-transport-root-9628f645b/products
```

The actual CLI uses 180 CPU seconds/3 GiB address space for generation and
120 CPU seconds/3 GiB for reading. These are experiment ceilings, not frame or
runtime budgets. Its existing internal solver/geometry limits also remain.

## Fresh checks, 2026-10-18

Initial generation/reader: `8f395a767440c622e263a44827694d5c1fe04612`.
The isolated delivery `9628f645b` carries exactly the same three root files on
master `76f679351`; unrelated unfinished prepared-growth CLI work is excluded.

Recipe SHA256: `c2388f7a0d159b1219ad293101dd3892e5e57d49753f850bd5521c16317006b1`.
The versioned Wiener MVT and four cropped DEM windows suffice: no network or
old private generated state was used. Original source IDs and attributes remain.
The 32 whole sources contain three stairs/24 risers and 16 protected pair
profiles. Fresh independent road and terrain readers pass; native
`admitted_source_ids` remains empty.

Wheel SHA256: `46a35a571e81c27e98e852466ff5e666068d675089684ac55ebee3b840d8b9b7`.

Source and installed runs match every array, shape, dtype and byte in all seven
saved NPZ products; source catalogs are byte equal. A third full generation in
comparison mode against the actual verified reconstructed state also passes its
reader and matches the same seven products. The separate allocation-traced generation on `9628f645b` also matches every
array and its saved reader passes. Independently changed real baseline
body, stale verification binding and mismatched actual network/terrain paths
are rejected. Three new mode/baseline controls and 22 existing whole-cycle
controls pass; the broader affected selection passes 37 controls installed
and 35+2 source (the first source invocation lacked `lib/tests` on its path;
only that missing two-test module was rerun with the correct path).

Fresh road: 4,536 vertices/9,108 closed-body faces. Terrain: 24,746 vertices/
49,488 closed-body faces in Double and Float32, no zero-area faces, all vertex
links cyclic/connected. Existing whole contact, protected clearances and full
Float32 contact envelopes pass. Unchanged support planning independently
accepts 119 fixed/124 shifted piers; its saved reader passes.

Blender 5.2.2 LTS Workbench rendered the actual bound arrays at 1280×720.
The whole root and whole root with supports were both opened. Coverage is sparse,
road contrast weak and terrain coarse: this restores a reproducible research
baseline; it is not a visible quality improvement or world-completeness proof.
No image, gallery or server is published.


| Exact artifact | SHA256 |
|---|---|
| `after/body.npz` | `b7b804a1e4f2c3b4ffd7326a3fc6fdb825f1879edc6004f1946bb7858bc7d4e3` |
| `after/display-body.npz` | `f7d15017a73e2e25016d7dbe509a406c6a8fba4677b75725ac57d15e27716faf` |
| `after/height-input.npz` | `bda8c4922446ca070629ef34e73937ef156ce2a1b361914f1f83dfaa3a173129` |
| `after-display.npz` | `57b832ecf464ea94dac1f49cd8da18d10300adf86ceb0b7aca0ad0bab27ad309` |
| `membrane-body.npz` | `bd04dd9a8116393326588358b9898ed8776f7c219b2b152dcc2c5fb61405f5da` |
| `membrane-field.npz` | `a0e97f9f4af787f067b6070945a406649fb7bb16ec934f3c30aff37822c025c6` |
| `terrain-plan.npz` | `5bc0f5cf6ce5a7f2325a3c0fd9cf693c26184443105329cd1be26839ae40255c` |

## Cost scope and remaining work

Measured on Linux Debian 13 x86_64 / Intel i5-1135G7, 4 cores/8 logical CPUs,
32 GB host RAM, shared development load; BLAS/OMP one thread. Uninstrumented
source/installed/comparison generation takes 29.42/32.56/29.46 seconds respectively;
these runs used nice 10 and were not isolated performance comparisons.
Source reported peak RSS is 272,344 KiB. Original sparse road solve arrays hold
3,163,216 bytes; its explicit quantized-XY copy accounts for 49,464 bytes.
These are known arrays/copies, not total process allocation or internal copies.

Separate complete generation under Memray 1.20.0, Python allocators/native
traces enabled after imports in the producer process: 122,341,224 positive-size non-free allocation
requests (the native statistics API counts 122,348,308 events),
13,519,785,213 cumulative requested bytes and
216,588,098 tracked peak live heap bytes. The instrumented
run took 154.44 seconds before display export and reported 343,800 KiB RSS;
profiling overhead is not normal execution time. Heap peak and process RSS
are different measurements and are not added. Requested bytes include repeated
requests/reallocations; they are not live bytes or measured copies. The entire
root's allocation pressure remains substantial; this delivery makes no speed,
memory optimization or target-device budget claim. Complete internal copy totals
and native device costs remain unmeasured.

Raw allocation capture SHA256:
`71594e3194fa4b12b987bf237175d7d56f1d5d8516461da3020c328391fd2a6f`.
Raw capture, the complete producer wrapper `profile_reconstruction.py`, equality
proof `reconstruction-equality.json`, saved readers and terminal logs remain in
the private directory below. The wrapper calls `generate(args)` once with the
same CLI limits and reconstruction fixture inside
`memray.Tracker(..., trace_python_allocators=True, native_traces=True)`;
module imports precede the tracker. Source arrays are independently re-read and
compared outside profiling.

Private current products, raw receipts, complete equality checks and images:
`/tmp/outshine-transport-resume-1f10d55eb/`. A reboot removed the older private
102-source snapshot, raw timings and images; their versioned reports remain
historical evidence. This root does not newly qualify that lost 102-source
state, reconstruct regional Draft #720, prove complete OSM generation, admit
all missing sources/levels or satisfy native/A18/RTX device budgets.
Continue #749 with freshly prepared complete states and real additional groups;
#434/#50 and the full research goal remain active.
