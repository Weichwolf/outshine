# Scoped window-depth reference (#712)

Existing real openings, open floors and stairs disappear behind pale panes in
the normal Python Eevee rendering. This experiment adds an **optional** thin-sheet
comparison, retaining the actual closed pane bodies. The default remains
`source`, including the agreed daylight. No native shader, shared material
binding or library contract changes. General glass quality remains open in #712.

## Model and independent numerical reference

`thin_glass.py` implements the unpolarized lossless thin-sheet model. With
air-to-glass interface power reflectance `f`, the internal ray series gives
`T = (1-f)^2 sum(f^(2k)) = (1-f)/(1+f)` and `R = 2f/(1+f)`.
At IOR1.52, normal-incidence R/T are0.081681972/0.918318028. At60° they are
0.169307404/0.830692596; at grazing incidence they approach1/0. IOR1 transmits
fully, including the limiting grazing case. Invalid arguments reject explicitly.

Five numerical controls compare the closed form with explicit successive
internal ray visits, energy/end cases, Snell-geometry offsets and the installed
Mitsuba3.9.1 `llvm_ad_rgb` `thindielectric` BSDF:80 actual delta samples across
four IORs, five angles, two sides and reflection/transmission branches. Branch
probability tolerance is2e-6; direction and unit transport weight/eta are checked.
This validates the chosen scalar model, not the Eevee image or full dielectric
physics. Averaging polarization before the nonlinear internal series differs
from tracking the two polarization components through the slab.

`blender_window_glass.py` uses the supplied world-space normal for interface
Fresnel and computes `2f/(1+f)` in nodes. An entering/front face mixes straight
Transparent BSDF with a white metallic GGX reflection at roughness0.02. Exit/back
faces are transparent so the closed body is not counted as two whole sheets.
This adds eight nodes to one shared fallback pane material; no per-pane geometry
or separate material instances. It is a smooth-sheet coefficient with a small
finite reflection lobe, not an exact rough dielectric or smooth delta shader.

Transmission has no absorption, colour, displacement, polarization state or
coherent interference. Parallel-slab displacement perpendicular to the outgoing
ray is bounded by thickness only for an infinite parallel slab. For the actual
8mm pane it is4.155mm at60° and6.697mm at80°. This is **not** a0.5px guarantee:
near views, occlusion changes and finite pane edge exits need separate treatment.

## Binding, geometry and cameras

`window_glass_probe.py` reads the existing versioned complete provider fixture
`data/stepped-profiles.json`, source
`Paris/(14, 8298, 5635)/5335130/11/72`, and builds its existing coupled cascade
prototype. It is a laboratory control, not a changed ordinary owner in the city
selection. There are41454 triangles, including3000 pane triangles. All six draw
arrays exactly replay the older coupled prototype: positions, topology, normals,
UVs, material IDs and facade extents. Near-camera selection now uses the nearest
eligible world window; it differs from the older private diagnostic camera.
No old image is relabelled as a fresh render of this camera.

One NPZ serves near, straight-front, grazing, inside and whole-building views;
each camera report points to its identical mesh hash. A8mm pane follows directly
from `terraced_facade_details.details` rectangle offsets−0.079/−0.087m. The
original model and its previous physical qualification remain distinct from
this optical comparison; there is no new independent controller/collider proof.

Only imported fallback `component-3` changes under an explicit comparison profile.
An explicit source material definition with ID3 is preserved; attempting to use
the comparison without that fallback role rejects. Opaque facade masks are not
silently made transparent. Profiles are:

| Profile | Purpose |
|---|---|
| `source` | Original Principled transmission, roughness0.12, IOR1.52 |
| `clear-principled` | Same material with roughness0.02; isolate roughness |
| `screen-clear` | Roughness0.02 plus explicit Eevee screen refraction |
| `thin-slab` | First-face sheet model with straight transmission |
| `transparent-control` | Retained geometry, all-pane Transparent BSDF control |

The last profile is a geometry/depth control, not a glass solution. Eevee uses
DITHERED transparency and transparent shadows. Thin glass requests no screen
raytracing. Output paths resolve beneath an explicit subdirectory of `/tmp`;
`/var/tmp`, `/tmp` itself and paths escaping through `..` reject before input IO.
Blender5.2 creates material node trees automatically; the importer only enables
nodes when a tree is absent, avoiding the deprecated setter on that version.

## Actual image result and scoped costs

Producer/renderer freeze: `5f1c9b724`. All13 actual1920×1080 images at16 samples
were optically opened: five source/thin-sheet pairs and three additional near
controls. Input/profile/triangle/image hashes are verified. The new optional
sheet reveals the opposite facade and real room depth. Interior-to-exterior
viewing also works; the grazing view reveals depth with strong grain. Lowering
Principled roughness alone stays pale/opaque-looking. Explicit screen refraction
reveals depth but distorts/blurs the opposite wall and openings. The transparent
control reveals clear room geometry without the sheet's reflection grain.

The sheet is not visually finished. DITHERED grain is obvious in near/edge views,
the whole-building gain is modest, and the repeated pale facade/room programme
remains monotonous. This experiment does not solve architectural diversity.
There is no default promotion. Night/local-light, repeated variance/temporal,
finite-edge displacement and general radiance/shadow checks remain open in #712.

Linux x86_64, four CPUs/16GiB-class host, Python3.13.5, NumPy2.5.4, bpy5.2.2LTS,
Eevee/Xvfb, **llvmpipe LLVM19.1.7**, not RTX/Cycles. Times below include actual
render work and possible shader compilation; they are single captures on a
shared host, not a repeated controlled performance ranking. Small normal-pixel
tests overlapped the first thin render; unrelated lint/build activity was seen
during the final worker. Setup times and per-worker cumulative peak RSS are in
each original report. RSS across scenes/processes must not be added or treated
as each scene's live allocation.

| View | Source render s | Thin-sheet render s |
|---|---:|---:|
| Near |30.649|94.874|
| Front |28.798|99.560|
| Grazing |28.920|123.394|
| Inside |25.034|90.969|
| Whole |79.046|62.734|

Near controls: clear-Principled27.937s, screen-clear90.527s, transparent106.791s.
Recorded process cumulative peaks range1928688–2346400KiB; final-worker peak
includes earlier scenes. This is costly software reference rendering, not a
native frame or device measurement. Geometry work is unchanged:41454 submitted
triangles,3000 physical pane triangles; one material graph changes. Its first
front/back handling preserves all source draw arrays.

`test/scripts/pixels.py` source/whole versus sheet/whole:48664/2073600 pixels
differ (2.3468%),46370 by more than1/255, mean RGB channel difference0.533252/255,
maximum97/255. No black or near-black pixels in either image. These are intended
shading changes plus sampling variation, not a geometric/LOD error bound.

Ten current numerical/binding/path controls pass at `299c0b578`; two existing
actual Eevee normal controls pass at `5f1c9b724`. The only intervening Buildings
Python change adds the `/tmp` output rejection control; other merged work is
Vegetation-only. No `lib/` change or fresh installed-library claim.
An initial private batch omitted its profile argument: the reports correctly
recorded `source`; wrong-folder duplicates were removed. A resumed long worker
ended143 after nine valid captures. The fresh remaining/profile worker ended0,
then all13 report/image hashes were verified. Interrupted runs are not passing
cohort gates. Original per-image provenance remains intact.

Current local evidence: `/tmp/outshine-buildings-window-models-5f1c9b724/` contains
one NPZ, five camera reports,13 images/original render reports, `assessment.json`,
`references.json`, original-array replay, analytical cases and pixel comparison.
The frozen worktree is disposable after integration; these inputs/images remain
local. There is no gallery or server.

## Reproduction

Use the project `.venv` and its pinned bpy5.2.2/Mitsuba3.9.1 requirements. On a
clean frozen commit, with outputs kept exclusively in `/tmp`:

```sh
export TMPDIR=/tmp
export PYTHONPYCACHEPREFIX=/tmp/outshine-buildings-pycache
export XDG_CACHE_HOME=/tmp/outshine-buildings-cache
export MPLCONFIGDIR=/tmp/outshine-buildings-mpl
export PYTHONPATH=lib/src:test/experiments/buildings:test/experiments/assets
export OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1
.venv/bin/python -W error -m unittest verify_thin_glass verify_window_glass
nice -n 10 xvfb-run -a .venv/bin/python -W error -m unittest verify_city_normals
nice -n 10 .venv/bin/python test/experiments/buildings/window_glass_probe.py \
  --output /tmp/outshine-buildings-window-models
for pose in Near Front Grazing Inside Whole; do
  for glass in source thin-slab; do
    nice -n 10 xvfb-run -a .venv/bin/python \
      test/experiments/buildings/blender_city.py \
      /tmp/outshine-buildings-window-models/Window${pose}-geometry.json \
      --output /tmp/outshine-buildings-window-models/images/$glass \
      --samples 16 --glass-profile "$glass"
  done
done
for glass in clear-principled screen-clear transparent-control; do
  nice -n 10 xvfb-run -a .venv/bin/python \
    test/experiments/buildings/blender_city.py \
    /tmp/outshine-buildings-window-models/WindowNear-geometry.json \
    --output /tmp/outshine-buildings-window-models/images/$glass \
    --samples 16 --glass-profile "$glass"
done
```

The supplied material definitions, camera, all triangles and agreed
`research-daylight-v1` are identical within each pair. Each actual image report
records the requested glass profile, input/implementation hashes, actual source
triangle count, Blender/backend, settings, image hash, setup/render times and
process cumulative peak RSS. Reports must be checked, not inferred from folder
names. There is no native/device/frame/cache/LOD, motion or night admission.

## Sources

First local SIGGRAPH check: Burley, *Physically-Based Shading at Disney*,2012
course, local v3 revised2014, [PDF](../../../doc/references/materials/siggraph/2012-disney-physically-based-shading.pdf),
pages15/16 optically opened for GTR/GGX, roughness mapping and Fresnel direction.
It does not validate this thin-sheet shader. The independent installed Mitsuba
delta BSDF is the scalar-model oracle. The locally installed EsriSDK3.4.12206
`doc/html/cgaref/cgareference/attr_material.html` exposes PBR/opacity/reflectivity
attributes; that list does not establish a thin-dielectric rendering algorithm.
The earlier stochastic-transparency reference and its scope remain recorded in
[the coupled-city report](terraced_crowned_city.md). Foreign implementation code
is not bundled. Local reference hashes and oracle/version scope accompany the
actual render inputs.
