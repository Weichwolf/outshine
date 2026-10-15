# Bounded rail hardware: four-place comparison

Owner: [#250](https://github.com/Weichwolf/outshine/issues/250), under #19/#50.
Measured 2026-10-15 on clean frozen source
`e4e9733aff839459c51407505ae89ebe039cc13d`. Python reference only;
native integration/device/render gates remain #11/#48.

## Result and geometric scope

The same captured four camera windows now carry closed rail profiles and shared
sleeper instances on explicitly selected, verified **horizontal** beds. Existing
source usage/IDs and every original displayed vertex, triangle and material slot
are preserved exactly, including the old Vienna C1 window. Fixed camera scene
parameters are unchanged. No input acquisition or place-specific geometry was
added. Whole windows are rendered; geometry outside the existing windows and
missing world classes remain absent.

| Place | Accepted pieces | Accepted centreline m | Sleepers | Complete footprints |
|---|---:|---:|---:|---:|
| Wien | 72 | 11303.425 | 18849 | 18993 |
| Tokyo | 96 | 8191.791 | 13663 | 13855 |
| Central Park | 4 | 1102.849 | 1838 | 1846 |
| Feldkirch | 150 | 15324.799 | 25560 | 25860 |
| Total | 322 | 35922.865 | 59910 | 60554 |

These are optional hardware coverage figures, **not** complete railway-network
coverage. Initial eligible chains: 62/60/6/132. The source-tag gauge is absent in
all accepted cases: 1.435 m is an explicit procedural assumption everywhere,
including Tokyo. The simplified own 12-point section uses 72 mm head, 150 mm
foot and 172 mm height; sleepers are 2.6×0.24×0.18 m at target spacing 0.6 m.
The inner head-face distance is the gauge; rail centres are gauge+72 mm apart.

Branch/crossing exclusion is conservative and source-preserving: 6 m around
branch nodes, plus full carrier/land width guards. Local failures comprise three
sharp-corner pieces, three short pieces and one actual overlapping Tokyo piece
(12.04084154 m). The committed `fixtures/rail-overlap-tokyo.json` captures that
real clipped counterexample with its source IDs. Admission retains the earlier
valid hardware and drops only the conflicting optional piece; existing beds and
source records remain. Two-dimensional rail-foot/sleeper overlap is permitted
only as a zero-volume vertical contact, never as a rail/rail or sleeper/sleeper
volume intersection. No turnout, active level crossing, fastening, rail tilt,
train trajectory, structural load, vertical railway profile or coupled LOD claim.

## Independent verification and provenance

`rail_place_verify.py` reconstructs the actual finite beds from all pinned mesh
triangles, checks their domain equality (0 m² error in all four cases), whole
hardware footprints, original source-axis support, oriented closed solids,
section volumes, gauge, material assignments and height contacts. It reconstructs
**all** sleeper vertices independently from saved instance transforms: maximum
vertex error 0 m. Maximum gauge error is 4.792e-13 m in the historical local
scaled-Mercator plan frame. Presentation transforms bed and hardware together to
WGS84 ENU; this does not create a separate ENU gauge/contact certificate.
Same-kind pair candidates: 94389/68507/9198/128100; no surviving volume conflicts
or land-width-band hits. Hash-checked original source records remain untouched.

Inputs are the retained offline #209/#232 camera products, generation SHA256
`514959867d425d5925ce30360870664a5aa0ae039ec33f5aae4a3c1a12d31dc7`.
Per-place original-input SHA256:

- Wien: `9e29075c947525d29a6428e9edadea070764f062a9992cd413fe87709cb6d6cc`
- Tokyo: `8740b09ecc9ad5bbc0346a792e6f360359a6ddd866b7ab0da4eaad19e7313c70`
- Central Park: `c9b8fd1b99ed98b85143289f83fc5b15ae50c4cfc40c2e817e7e301c47e6e30d`
- Feldkirch: `cdb6bb2cd28020d992c8512e621b21add0c2643da389e85ad6c9d3e59728694f`

Fresh output generation SHA256:
`9363af8915eefb2a4fd36feca42d5b3e340a666b1a5ed919983c7d732d6af7d1`.
It binds every experimental rail implementation and the actually imported
library files. Strict verification binds this generation hash; the renderer
requires that clean, complete verification and checks every payload hash.

Frozen source: all 353 library tests pass using the existing Buildings environment
with its optional extras; 35 focused transport/library controls pass independently
in the Transport environment. The installed wheel passes the same 35 controls.
All 163 runtime package files (documentation README files excluded) equal the
frozen source bytes; every one of the four installed-wheel NPZ outputs is byte
identical to the direct-source output. Installed module paths were checked.
Wheel SHA256: `6e2101b8ad87dc10803e8d88129f07e08075accd96fdf08466bd89c2cd45ffc2`.
Ruff check/format and the 120-line function limit pass for the added Python files.
An initial `--no-build-isolation` wheel command failed because the Transport env
lacks setuptools; the normal isolated pip build passed without product changes.
A first package comparison included an intentionally unpackaged documentation
README; the corrected comparison covers all runtime files. Neither failed
supervisor step is reported as a passing gate.

## Equivalent repetition cost

Same 59910 real transforms; SHA256
`42a66d5dde99bdac023a273bd40da520269f979a9f04f6bd6ffe2e2a16037f82`.
NumPy2.2.4/Python3.13.5, one BLAS/OMP thread. Three fresh workers per mode,
seven representation constructions per worker; phase medians over 21 calls.
Input decoding, rail construction, footprint admission and rendering are excluded
from these phase measurements. Baked mode uses the actual public, validated bake
API; compact mode copies already verified matrices into its owned readonly buffer.
This is the representation choice for known valid inputs, not a general matrix
validation benchmark. Each phase discards its previous output before construction.

| Work per construction | Shared sleeper + instances | Baked geometry |
|---|---:|---:|
| Output payload B | 5751696 | 20129760 |
| Explicit owned-output copy B | 5751360 | 20129760 |
| Expanded vertices | 0 | 479280 |
| Remapped triangle indices | 0 | 2156760 |
| Logical transform multiplies | deferred | 4313520 |
| Phase median ms | 0.578 | 105.021 |
| Observed phase range ms | 0.452–3.253 | 87.882–162.689 |
| Fresh worker peak RSS B, max of three | 52834304 | 87257088 |
| Profiled positive allocation events | 14 | 237 |
| Profiled cumulative requested B | 5752134 | 43868060 |
| Profiled tracked heap peak B | 5751784 | 40263792 |

Memray1.20.0, `native_traces=True, trace_python_allocators=True`, separate one-call
profiles after input decode/imports. Allocation events include positive realloc
requests; cumulative bytes are not unique/live memory. Internal backend memcpy
bytes remain unobserved. Explicit copied bytes count only owned output copies;
shared 336 B unit mesh is counted once. Compact output uses 71.4% less payload.
Deferred GPU transforms, rasterization, shadows, collision, upload, selection and
LOD still cost work and have not been benchmarked. No FPS/device claim follows.

Frozen direct-source four-place generation: 8.713 s wall /109.1 MiB child peak RSS;
independent full verification: 13.831 s /304.4 MiB. Installed-wheel generation:
8.860 s /110.0 MiB. These include worker input/output, are offline Python jobs,
and are not native asset-cache load times. Generator limits: 2 GiB address space,
60 CPU s overall, a 15 s cooperative check between per-window hardware builds,
2048 pieces;
per-product defaults 4096 stations/8192 sleepers/98304 rail vertices. Pair caps,
batch sizes and absence reasons are in the public API documentation and receipts.
Only per-process RSS is reported, not summed process-tree/OS/GPU memory.

The minimal parent uses `Popen` plus `os.wait4` for terminal status, CPU time and
RSS. Directly inherited launcher high-water RSS was discarded as a measurement
artifact. Fresh measured workers run at normal priority; competing foreign Python
jobs and load snapshots are recorded. Builds/rendering run at nice10; costs are
this loaded-host observation, not isolated hardware capacity or portable timing.

## Sources, pictures and reproduction

SIGGRAPH first: Chen et al.2008, PDF page2/Fig2 (graph/attributes before separate
3D geometry); Galin et al.2010 (terrain/road cross section). Existing papers remain
under `doc/references/transport/`. The manufacturer's [60E1 drawing](https://www.voestalpine.com/highperformancemetals/argentina/app/uploads/sites/264/2024/09/voestalpine-60E1.pdf)
was optically inspected, as was Chen page2. Downloaded PDF SHA256:
`bff8d502dc8b51421a843dceb58f51c0e2153101bed92ce887dd23fed404238d`.
It supplies dimensions, not copied code or the exact curved section. No SDK calls
or dependencies are involved in this delivery; previous ESRI operation research
remains separate and does not prove a railway/OSM generator.

The four complete fixed-camera Workbench PNGs are 1280×720, Blender4.3.2,
two rendering threads. Each new PNG was generated in a fresh external output tree,
format/dimensions/hash checked and opened against its previous PNG. Tokyo visibly
resolves rails and sleepers in the foreground; the other cameras have smaller or
distant rail coverage. These are baked geometry inspection pictures with simple
materials, not a native or physically based renderer acceptance. Existing missing
terrain/world content remains visible. Research images now remain local in system-temp
directories; only master publishes integrated Places. The former research gallery is retired.

Reproduction commands are in [README.md](README.md), rail section, using the
single public kernel `outshine_ref.transport.rail_hardware`; experiments contain
no parallel construction algorithm. Raw generation/verification, wheel equality,
normal/profiled costs, process/load records, image receipts and compact base-prefix
proof are retained externally in
`/tmp/outshine-transport-rail-evidence-e4e9733af/`. Needed pinned source products
remain in `/var/tmp/outshine-transport-place-camera-inputs-209/`; existing source
caches, previous required plans and before-images are retained. New private work
uses `/tmp` on the root ext4 filesystem; no tmpfs capacity assumption remains.

Final images and loaded-host render observations (two threads, nice10):

| Place | Changed pixels /921600 | PNG SHA256 | Render wall s / child peak MiB |
|---|---:|---|---:|
| Wien | 4006 | `3f780d14cb2021a0cbf5a69ee1216e52959914c913a7d225e0d8e67ce50e9444` | 39.530 / 1083.5 |
| Tokyo | 26101 | `2b4cbac185e0295aaecfcef7b4db49968f123c9f8f28918853a697da2016f921` | 40.733 / 958.7 |
| CentralPark | 229 | `a54379037c768bcb55f89c2b983885258d538877ee5ac10cab515dc7f0d35a6e` | 70.996 / 1233.6 |
| Feldkirch | 1816 | `6837fd6987d529fa9c9b69cceab33456302af5134a18047741c90d01216d9a49` | 46.642 / 1032.8 |

Feldkirch first reached the 90 s preview-process guard under competing load,
without saving a PNG. One unchanged-scene retry passed the same 90 s guard.
The failed run remains in the process receipt; no timeout or product budget was
raised. Render costs are offline inspection costs, not frame timings.

After merging compatible master changes, frozen `8c0b46bb0` passes all 357
library tests and 35 focused controls. Every rail implementation, shared Mesh
contract, renderer and scene-preparation file is byte-identical to the measured
`e4e9733af` sources; geometry, wheel, cost and image proofs remain applicable.
Only subsequent report edits accompany this integration check.
