# Stable ceiling lights and night-window controls (#712)

The existing coupled cascade prototype now has camera-independent ceiling
fixtures and separate all-lit/mixed/off states. Actual near and whole-building
night images expose its real open floors through the optional thin glass.
This is one existing provider control, not a new architecture family or a
production night renderer. Source glass and common daylight remain the defaults.
The scalar glass model and earlier day/angle controls are in
[window_glass.md](window_glass.md).

## Geometry, fixtures and state

`window_glass_probe.py` reads the versioned `data/stepped-profiles.json` source
`Paris/(14, 8298, 5635)/5335130/11/72`. All six draw arrays replay the prior
control exactly: 41454 triangles, including 3000 pane triangles; NPZ SHA256
`64b77527194dbea0233049d20fc69c8d91a192faaa6f5333d197100978ca7a50`.
Body, roof, stairs, openings, normals, UVs and collision geometry remain.

`open_floor_lights.fixture_plan` uses the same core-local frame and existing
open-floor cavity IDs. Full 0.8m fixture rectangles must fit inside a cavity,
outside a 0.20m stair-core keepout. An empty grid gets one smaller contained
fixture. Positions lie 0.06m below the ceiling, then transform once to the input
frame. Positive integer candidate budgets reject before grid expansion; default
4096. This case checks 140 cells of an 8m grid and emits 64 descriptors across
five existing floors. No new room subdivision or camera input.

IDs combine cavity identity and integer grid coordinates. Each floor has one
declared warm/neutral RGB preset. `light_state` changes only enabled flags:
all-lit enables 64 lights, mixed 48, off 0. Windows share the state of their
actual connected floor. This is a diagnostic policy, not measured occupancy.
Descriptors and state stay separate; no baked room radiance or state-driven
body rebuild. Independent world-space checks cover full rectangles, core
avoidance and height above a 1.90m actor, not a new controller/collider proof.

`blender_room_night.py` binds downward rectangular area lights: 160 Blender
radiant W each, normalized by area, light exposure 0. These are not electrical W,
calibrated lumens or measured CCT. Light objects have no visible fixture meshes.
Sun energy is 0; world strength 0.003 retains the daylight basis RGB. AgX and
camera exposure 1.5 remain. This is a local diagnostic, not astronomical sky/GI
or a shared native profile. Actual positions match the intended Float32 copy;
maximum rounding is 2.98758e-5m. No regional GPU/precision contract follows (#717).

## Actual captures and resource refusal

Producer: `95f8b829e`; corrected renderer: `f50b82cd8`. All six fresh 1920×1080
images at 16 samples were optically opened. Actual source/report/image hashes,
profiles, dimensions and 41454 imported triangles were checked. Fixture plans
remain identical across states and cameras. The separate worker ends 0; no
shadow-pool overflow, allocation or shader-compilation error appears in its
per-image logs. Process 0 alone is not admission.

Near-all shows actual illuminated walls/floors; near-off becomes dark. The
whole mixed view has coherent light/dark floors and warm/neutral responses.
With source glass, the same 64 active lights remain mostly hidden. The outside
is intentionally very dark without street/astronomical lighting. These are
simple empty floors, not furnished rooms or complete night realism.

An earlier `ae16206cf` attempt ended 143 without a PNG. A `95f8b829e` near-all
capture then saved an image but requested about 3134 shadow pages against 2048
available; it is rejected despite process 0. Its remaining group was explicitly
stopped. The corrected night profile sets Blender's shadow-pool setting to
1024 instead of 512, retaining all lights, samples and shadow-resolution scale
1.0. This supplies reference capacity; no Outshine device budget is raised or
satisfied. Failed images/logs remain separate counterexamples.

## Scoped costs and controls

Linux x86_64, four-CPU/16GiB-class host, Python 3.13.5, NumPy 2.5.4, bpy 5.2.2LTS,
Eevee/Xvfb on **llvmpipe LLVM19.1.7**, not RTX/Cycles. These are single shared-host
captures, not an isolated repeated ranking. Each row has a fresh image worker.
RSS is its lifetime peak including imports, source buffers and software-driver
resources; do not add peaks or equate the pool setting with measured usage.

| Capture | Active lights | Render s | Setup s | Process peak KiB |
|---|---:|---:|---:|---:|
| Near, thin glass, all-lit |64|867.977|0.441|2683996|
| Near, thin glass, off |0|121.109|0.516|2454584|
| Whole, thin glass, mixed |48|387.929|0.754|2569428|
| Whole, thin glass, off |0|61.425|0.593|2511400|
| Near, source glass, all-lit |64|97.416|0.775|2478608|
| Near, screen-slab, daylight |0 local|121.275|0.969|2152888|

Setup starts after report parsing/source hashing and includes array import and
scene/camera binding. These costly renders are not native frame timings.
`pixels.py` whole-off versus whole-mixed: 68802/2073600 pixels differ (3.3180%),
55501 by more than 1/255, mean RGB channel difference 1.549108/255, maximum
225/255. Intended lighting change plus sampling, not a geometry/LOD bound;
black/near-black pixels in the off state are expected.

Fixture-only measurement on `2a74d4d3b`: 16 calls, median 5.603ms, p95 8.363ms;
mass-plan setup 54.498ms separately, no body mesh build. Retained JSON 26658B;
tracemalloc retained 38761B/peak 49575B; worker peak 53560KiB. Cell counts are
derived from the same domains. Total allocation churn, Shapely-native peaks,
copied bytes and native/device costs remain unmeasured. No cache-hit claim.

Sixteen controls on `f50b82cd8` pass: independent fixture containment/headroom,
stable identities and reordered state resolution, invalid/budget cases, actual
Blender light/position/power binding, glass coefficients/Mitsuba samples and
output-path constraints. Integration `5680564df` preserves all Buildings/library
Python files and repeats the same 16 controls. No `lib/` change or fresh
installed-library claim.

## Alternatives that remain counterexamples

The new `screen-slab` comparison binds the source-declared uniform 8mm thickness
and Blender's infinite-slab approximation. Its actual day image still distorts/
blurs the opposite geometry; it is not promoted. Missing/invalid thickness
rejects. This does not fix screen-space visibility limitations.

`window_blend_order_probe.py` renders two emitting/transmitting planes in one
mesh, both triangle orders and both camera sides. Independent CPU source-over
composition is the oracle. All eight actual 64×64 unit images on `ae16206cf`
were opened. BLENDED fails two cases by up to 0.193228486, about 49.27/255 per
channel; DITHERED centre means at 64 samples differ by at most 0.004373639.
One finite mean test is not a variance/temporal or physical-glass proof.
Direct BLENDED is unsuitable for an unsorted shared mesh the player walks around.

An actual day near image at 64 instead of 16 samples (`2480e182e`) was opened.
Removing an affine RGB8 field from a fixed smooth floor patch gives residual
RMS 2.165→1.164; a structured patch changes 4.718→4.356. These are single-capture
display residuals, not pure sampling variance. Render 546.517s with concurrent
host work; no fair speed ratio to the earlier 16-sample capture. More samples
do not eliminate the quality/cost problem.

## Reproduction and evidence

Use pinned `.venv`, a clean commit and explicit `/tmp` outputs:

```sh
export TMPDIR=/tmp
export PYTHONPYCACHEPREFIX=/tmp/outshine-buildings-pycache
export XDG_CACHE_HOME=/tmp/outshine-buildings-cache
export MPLCONFIGDIR=/tmp/outshine-buildings-mpl
export PYTHONPATH=lib/src:test/experiments/buildings:test/experiments/assets
export OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1
.venv/bin/python -W error -m unittest verify_open_floor_lights verify_thin_glass verify_window_glass
nice -n 10 .venv/bin/python test/experiments/buildings/window_glass_probe.py \
  --output /tmp/outshine-buildings-night-models
while read -r pose glass label; do
  nice -n 10 xvfb-run -a .venv/bin/python test/experiments/buildings/blender_city.py \
    "/tmp/outshine-buildings-night-models/$pose-geometry.json" \
    --output "/tmp/outshine-buildings-night-images/$label" --samples 16 \
    --glass-profile "$glass" --lighting-profile open-floor-night
done <<'CASES'
WindowNearNightAllLit thin-slab near-all
WindowNearNightOff thin-slab near-off
WindowWholeNightMixed thin-slab whole-mixed
WindowWholeNightOff thin-slab whole-off
WindowNearNightAllLit source near-source
CASES
nice -n 10 .venv/bin/python test/experiments/buildings/open_floor_light_cost.py \
  --output /tmp/outshine-buildings-night-cost.json
nice -n 10 xvfb-run -a .venv/bin/python test/experiments/buildings/window_blend_order_probe.py \
  --output /tmp/outshine-buildings-night-order
```

For the day comparison use `WindowNear-geometry.json`, `--glass-profile
screen-slab`, 16 samples and default daylight. The 64-sample grain control uses
`thin-slab` and daylight. Reject resource errors in logs, check reports/hashes/
positions, then open images. No automatic oracle/pin update, gallery or server.

Local originals: `/tmp/outshine-buildings-room-night-shadows-f50b82cd8/` holds
six image/report/log triples and terminal job record. Source NPZ/cameras/light
descriptors and fixture costs stay in
`/tmp/outshine-buildings-room-night-models-95f8b829e/`. Day/alpha originals remain
under `/tmp/outshine-buildings-glass-sampling-2480e182e-results/`, final alpha
control in `order-frozen-ae16206cf/`. No specialist images enter `doc/`.

SIGGRAPH first: [Frostbite PBR notes 2014](../../../doc/references/lighting/siggraph/2014-frostbite-pbr-course-notes.pdf),
PDF pages 24/26 optically opened for light categories and radiometric/photometric
units, not Blender calibration. Installed bpy RNA confirms power/normalization/
exposure semantics. Additional primary source:
[McGuire/Mara 2014](../../../doc/references/lighting/jcgt/2014-efficient-screen-space-rays.pdf),
page 1 optically opened; no claim to implement its ray tracer. Earlier glass/
stochastic sources retain their stated scope. Local inputs include source hashes.

This completes a scoped ceiling/window/state reference, not the overall quality
goal. Facade/body/material variety, efficient room/light demand, robust night/GI/
reflections, finite-edge displacement, temporal, cache/controller/native/device
admission remain with #33/#32/#31/#10/#48 and the native specialist owners.
