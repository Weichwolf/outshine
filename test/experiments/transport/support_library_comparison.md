# Installable whole-contact support construction (#332)

The #326/PR #330 kernel depended on experiment/test helpers. The actual Vienna
producer now imports `outshine_ref.transport.supports`; the old construction
copy is removed. Its small compatibility adapter delegates to the library.
Independent reference surfaces/whole-contact/volume/source replay remain outside
the kernel. Existing public APIs and Plan/Reservation products are unchanged.

API and bounds: [lib/supports.md](../../../lib/supports.md). The former minimal
real fixture moved unchanged to `lib/tests/fixtures/bridge-support-contact-vienna.json`,
SHA256 `80fdf7426ecf83b35527137a8cdcfc2c025a3bdbb54207473bee7876138dc048`.
Construction remains the same shared original-edge arrangement and directed caps/
walls; no quantization or coordinate weld. Additional guards reject ambiguous
local source patches and preflight arrangement work before its emission.

First reference remains Chen et al., [SIGGRAPH 2008 page 2/Figure 2](../../../doc/references/transport/siggraph/2008-interactive-procedural-street-modeling.pdf),
then Galin et al., [Eurographics 2010 page 8/Figures 17–18](../../../doc/references/transport/eurographics/2010-procedural-generation-of-roads.pdf).
Both diagrams were optically inspected for the predecessor, with page 8 repeated
for #326. This migration does not reproduce either full paper; pier dimensions
and the conservative segment-pair bound are our explicit recipe/work decisions.

## Frozen source, installed wheel and real input

Final computational source: `9b64afdf2eb88db3a7fbc4f2ba94a34f1c6ce6a3`.
Fresh wheel SHA256:
`e6d78c78f5fb9f19cc3ec4f6a5696dd1f99e435c299c9c4b5bf04523d1658c6f`.
All four installed support module files match that source. Wheel controls run
with `python -I` in a fresh external environment, without an experiment path.
Full Vienna generation separately imports that installed wheel with only the
experiment caller on PYTHONPATH; the import path receipt proves its origin.

12 library controls cover analytic creases/interior peaks/holes, ownership,
missing/ambiguous/local contacts, work/time limits, pre-noding bounds, invalid
inputs and the actual Vienna counterexample. 50 source controls include those
plus shared surface/sampling/clearance and existing support experiments. The full
saved source and wheel results undergo independent whole-contact, positive
height/volume, source-station/recipe, mutual/exclusion and float32-body replay.

The exact original #303 source/ground pins and #326 bridge choices are retained.
Both methods preserve all 307 source-station decisions and the old emitted data:
262 fixed /288 shifted piers, 19 shifted exclusions, 6,364 selected body faces.
All arrays from the source build are byte-identical to #330. The installed-wheel
build has at most 3.553e−15 m Float64 positional difference; indices, materials and
supplied normals remain exact. Every Float32 presentation array is identical.
Both independent replays retain whole-contact error ≤1.706e−13 m, full footprints,
unchanged base geometry/materials and no pier/traffic/portal conflict. This does
not change the old 144 m support gap or establish safe structural spans.

## Work, lifetime and cost

Source planning: 1.257 s fixed /1.309 s shifted; prepared indices 0.264 s.
Installed-wheel planning: 1.185 s /1.298 s; preparation 0.254 s. Full comparison,
verification and exports: source 15.329 s /328.105 MiB worker peak, installed
wheel 14.487 s /372.465 MiB. These are shared-host observations, not speedup or
device/frame acceptance. Preparation and whole-process peaks are distinct from
the planning call. Original #330 generation observed 442.359 MiB peak.

An initial migration kept construction and reference indices simultaneously and
observed 528.699 MiB. The actual producer now drops construction indices before
independent reference preparation, then releases reference indices before large
body exports. Actual generated arrays remain unchanged. The working-set change
follows lifetime, not a smaller source network, view or acceptance threshold.

Both final Memray 1.20.0 probes terminate and reproduce their saved geometry
exactly. Cold planning follows decoding/index preparation; export and independent
replay remain outside tracking. A concurrent two-thread software-render process
was present, so instrumented wall times are not isolated CPU comparisons.

| Instrumented planning only | Fixed | Shifted |
|---|---:|---:|
| Allocation events |3,676,870|4,078,619|
| Cumulative requested bytes |411,347,103|453,598,126|
| Tracked peak heap bytes |14,532,313|14,532,313|
| Instrumented wall, s |6.748|7.350|
| Whole worker peak RSS, MiB |286.453|290.508|

The new pre-noding bound costs work: event counts are about 10% above the old
#330 probe (3,339,934 /3,706,092). It bounds potentially large arrangement
emission before allocation instead of silently relaxing that admission. Neither
the tracked heap nor cumulative allocation requests are whole-process live
memory. Final accepted-prism preflight pair counts are in each station's report.

Owned output mesh/cap bytes remain 150,336/110,208 fixed and 166,560/122,256 shifted.
The complete after-body payload remains 29,143,808 bytes. Its two known explicit
export copies are 30,845,952 indexed-XYZ bytes and 15,422,976 repeated-normal bytes;
these are partial copy counts, not all pipeline traffic. Source/element budgets,
pre-noding pair work and independent checks are retained. No source-frame/Kind/
Source consumer or native API is silently added.

Fresh installed-wheel 1280×720 and 1920×1080 after images were actually opened.
`test/scripts/pixels.py` reports zero changed pixels against #330 at both sizes;
PNG hashes differ, while decoded pixels match exactly. Actual #291 camera,
mesh/explicit-normal and daylight helper provenance is saved. Offline render
wall/RSS: 30.889 s /2,020.902 MiB at 720p, 44.004 s /2,174.676 MiB at 1080p.
Blender 5.2.2/Eevee/llvmpipe LLVM 19.1.7, 16 samples/two threads. Camera, palette,
lighting, finite source border and height scale stay unchanged. This migration
preserves the visible result; it does not claim a new visual improvement.

Current local results: `/tmp/outshine-transport-support-library-332-v2/` and
`/tmp/outshine-transport-support-library-332-wheel-result-v2/`. Wheel, fresh
environment, isolated tests and import receipt:
`/tmp/outshine-transport-support-library-332-wheel-v2/`. Array/station comparison:
`/tmp/outshine-transport-support-library-332-v2-identity.json`.
Old #330 and the necessary #303/source archives remain the reference inputs.
All pictures remain local. Native integration, structural recipes, consumer
binding and ground C1 remain open. Reproduce using the support commands in
[README](README.md), once with `PYTHONPATH=lib/src:...`, once with the freshly
installed wheel and only the experiment path. Optional allocation probe requires
Memray 1.20.0; packages/versions and actual renderer remain as declared in #326.
