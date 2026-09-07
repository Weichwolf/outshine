Type: chore
State: open
Area: include
Tags: measured, door

# A reader who knows Filament finds every door name where Filament puts it

**Benchmark** -- Unreal answers this with a prefix system and one class per file (`UWorld`,
`AActor`); RAGE with a two-letter subsystem prefix on every type (`grcTexture`, `fwEntity`).
The two agree on the property and differ on the mechanism, and this tree takes the third that
both of its door's sources take: a NAMESPACE plus one header per public type, grouped as
Filament groups `filament/`, `math/`, `utils/`. **All three agree that a public type is findable
by its own name**, and this door is not yet.

## Where it stands, measured 2026-09-04

| what | now | reference |
|---|---|---|
| `include/scenario/Scenario.h` | 708 lines, 51 top-level types | Filament: one header per type a client reaches for |
| `Geometry` | parts, surfaces, lamps | Filament's word for VERTEX DATA; ours is Cesium's `Model` |
| `Loaded` | reads a file, holds animations and cameras | `FilamentAsset` -> `Asset` |
| `Engine::setView(id)` + `Renderer::render(Extent)` | which view is engine state, how big is the argument | Filament: `Renderer::render(View*)` |
| `SwapChain::logsTo` | DECLARED at `Outshine.h:68`, defined nowhere | a dead door declaration |
| `Engine::logsTo` | `static`; the sink is process-wide | a free `outshine::logsTo` beside `LogSink` |
| a word declared twice | 9 type names, exactly at the claim's ceiling: `Node` `Document` `Attribute` `Value` `Scene` `Host` `Declaration` `Camera` `Sampler` | one meaning per word in `include/` |

Done and holding: `Camera::Perspective` / `Ortho` / `Exposure` as named records chosen by type
(glTF's shape, half-extents never edges); `sampleHeight(const LongitudeLatitudeHeight &)`.

## The cut, by what a client is doing when it reaches for the name

| header | holds |
|---|---|
| `scenario/Document.h` | the root, `Identity`, `Layer`, `Persisted`, `Binding`, `Clock` |
| `scenario/View.h` | `View`, `Camera`, `Patch` |
| `scenario/World.h` | `Georeference`, `Weather`, `Relief`, `Structure`, `WorldSettings`, `Provider`, `Setting`, `Generating`, `Compositor` |
| `scenario/Render.h` | `RenderPlan`, `Lighting`, `Light`, `SurfaceOverride` |
| `scenario/Body.h` | `Body`, `Drive`, `Prismatic`, `Slot`, `Standing`, `Placement`, `PhysicsSettings` -- after board:2127 has taken the tyre out |
| `scenario/Mind.h` | `Mind`, `Kind`, `Instance`, `Region`, `Door`, `Volume` |
| `scenario/Sound.h` | `Emitter`, `Voice`, `Sound`, `Room`, `Bus`, `Falls`, `Makes` |
| `scenario/Asset.h` | `Asset`, `AssetAnimation`, `Surface`, `Table`, `Event` |

`Renderer::render(Extent)` is the one STRUCTURAL question and the answer is written here so it
is not re-argued: the view stays engine state. A scenario DECLARES its views by id and the engine
owns the document; a `View*` a client holds would be a handle into a document it does not own,
and that is the reason ours differs from Filament rather than an oversight. `render(Extent)`
keeps the canvas as the argument because the canvas is the client's.

## What will be true

- [ ] Every public type stands in the header above; `Scenario.h` is an umbrella include and
      nothing else
- [ ] `Geometry` is `Model`, `Loaded` is `Asset` -- renamed at the declaration, callers named by
      the compiler
- [ ] `SwapChain::logsTo` is gone; `outshine::logsTo(LogSink *)` is the one door onto the sink
- [ ] The nine collisions are decided per word -- which meaning keeps it, what the other becomes
      -- and the claim's ceiling falls to 0
- [ ] `make doc` reports 0 undocumented entities in `include/scenario/` (board:2131 holds the
      rest)

## What will show I was wrong

A client in the tree -- `src/client/` -- that gets LONGER after the cut. The door is measured by
the client's line count and a split that costs the client includes is the wrong split.

## Convention audit, 2026-09-07 (user-requested; incomplete)

Corrected and tested against numerical glTF/KHR cases:

- `Camera::projectionMatrix` no longer requires a valid look-at target; it depends only on the lens.
- `Camera::modelMatrix` and its inverse use right-handed camera -Z/+Y, quaternion rotation including roll, or explicit look-at. `Engine::Watches` uses this same transform after resolving globe placement. The former quaternion branch had the wrong signs on forward X/Y and discarded roll.
- Positive infinite perspective far distance uses the analytic infinite matrix, not inf/inf. Invalid scalar inputs are refused.
- `UvTransformOf` retains its original signs: positive rotation is counterclockwise in glTF UV space (+V down). The attempted sign correction was WRONG. Khronos issue #1563 documents the erroneous GLSL listing in the extension README; Khronos Sample Renderer and Blender agree with the original implementation. The new numeric test was corrected against this independent evidence, not to loosen a threshold. For offset(0,1), rotation pi/2, scale(.5,.5), UV(1,0) maps to (0,.5) and UV(0,1) to (.5,1).
- `Mat4::Elements` names its column-major span; `Row` was false. `SurfaceMap::Sampler` replaces the unrelated `HeightSampler` name. Geometry and texture maps share `UvSet::Uv0/Uv1` instead of incompatible enums.
- Public comments distinguish Filament FoV degrees from glTF radians, generic XYZ from a georeferenced local frame, light intensity units, and native material defaults from glTF defaults.

Evidence: `make suite SUITE=outshine/conventions` failed against the pre-fix implementation and passes after the corrections (`build/front-door-before.log`, `build/front-door-*-check.log`). The test evaluates reference coordinates and analytic matrices; it does not claim complete glTF or Cesium compatibility.

Unresolved height-datum violation: `LongitudeLatitudeHeight` promises ellipsoidal height. `Engine::sampleHeight` and `Generators::HeightSampler::sampleHeightAslM` return DEM ASL values. Places assign `HeightAslM` directly to geodetic height; `Advancing.cpp` and `HeightSheets.cpp` feed ASL values into WGS84 ECEF conversion. There is no geoid conversion. Correct the provider datum boundary, not the WGS84 definition or a per-place offset. The source mosaic's actual datum must be established before choosing a geoid model. Public sampling comments now state the returned quantity honestly; this does NOT repair the conversion.

The remaining naming/layout work above is still open. No claim that the entire front door now meets Filament/Cesium conventions.

Sources: [glTF coordinate system and cameras](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html), [KHR_texture_transform](https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_texture_transform), [Filament Camera](https://github.com/google/filament/blob/main/filament/include/filament/Camera.h), [Cesium Camera](https://cesium.com/learn/cesiumjs/ref-doc/Camera.html).

### GPU and client boundary corrections

- The GPU now carries both orthographic extents independently of viewport aspect. The former `xmag == ymag * aspect` rejection expressed a renderer limitation, not a glTF constraint.
- Finite perspective far planes and orthographic near/far reach the GPU; the fixed 60000 m orthographic far plane is gone. Switching to perspective clears the previous orthographic extent.
- `Render::Lens::Projection` supplies both camera-relative MVP and aerial depth reconstruction. Reconstructing every depth as near/depth was only valid for infinite perspective. The shared coefficients now cover finite perspective and orthographic projection too. This does not validate every orthographic lighting/ray-origin assumption.
- `CameraOf` resets its output before writing a local camera, preventing stale globe anchors and settings on reused output values.
- `outshine-client run` honours the declared canvas. It reads the declaration before targeting; world composition now invokes the existing lazy `Stood` path, so declare -> canvas -> assemble works. Screenshot row statistics use the actual canvas size.
- Corpus translation now respects camera index, orthographic extents, and near/far. Nested camera cases are discovered. Explicitly requested missing cases fail visibly. `make corpus-prepare MANIFEST=...` prepares the pinned oracle. Missing PNGs can be encoded from the prepared oracle floats with the existing conversion, without replacing committed references.

Evidence: `build/front-door-projection-check.log` passes the public-camera and GPU-projection numeric regression, including independent extents on a 16:9 viewport, finite clipping planes and infinite reverse depth. `build/front-door-projection-render.log`: both camera cases pass; orthographic 100% within 8 codes (worst 2), perspective 99.9985% (worst 57). Both rendered PNGs and oracle PNGs were opened. UV cases remain red at five pixels each: TextureTransformTest 99.9870%, TextureTransformMultiTest 99.9895%, unchanged thresholds. Four of the former's differing pixels exchange black and 224 at edges; no blanket rasterization attribution is proven.

The original UV sign implementation is retained, backed by [Khronos issue 1563](https://github.com/KhronosGroup/glTF/issues/1563), [Khronos Sample Renderer](https://github.com/KhronosGroup/glTF-Sample-Renderer/blob/main/source/gltf/material.js), and Blender's glTF-to-UV conversion. The attempted opposite-sign version produced 6489 disagreeing pixels; correcting that mistake returns five. This supersedes any earlier claim that the original signs were a defect.

Places regression `build/front-door-places-check.log`: Husum e6bc1f75, p99 3.00 ms; Malcesine eebaea2a, p99 5.18 ms; both zero of 120 over 16.67 ms. Both PNGs opened; byte-identical to their pre-camera-correction renders. Shoreline teeth, cliff bands and excessive darkness remain visually unacceptable.

Additional material-boundary finding (fixed in the continuation below): `Loaded::Held::Names` transferred image, UV-set and sampler but dropped texture-transform properties when handing a `SurfaceMap` to callers. Public native map consumption and occlusion-map handling also require tracing; the internal imported shader path has its own transforms, so its vendor image does not prove public round-trip preservation.


### Public material handoff and corpus declarations, continuation

`Gltf::TextureRef` now retains the original `UvTransformProperties`. Matrices are derived at the rendering boundary. `Loaded` transfers those properties to all six currently resolved public texture sockets. No matrix decomposition loses negative or zero scales or the declared rotation. The pinned Khronos TextureTransformTest public-loader regression failed six of fourteen assertions before this repair and passes all fourteen afterward; the camera regression also passes (`build/front-door-material-before.log`, `build/front-door-material-after.log`). The internal UV render digests remain ee1bc7bf and 54342a70; both images were opened. Their existing five-pixel disagreements remain red.

Two independent corpus translation defects are repaired, without changing the oracles or thresholds:

- Manifest cameras with `projection: orthographic` and `yMagM` now reach the public orthographic camera, just as imported glTF cameras already did. An explicit horizontal half extent is retained; otherwise it follows from the declared viewport aspect.
- An explicitly uniform oracle environment now supplies `colourLinear * strength`; black means zero. Previously every metal-rough case received Blender factory grey, even when its manifest declared black or 0.25 white.

Controlled sequence: original NormalTangentTest 17.5676%, NormalTangentMirrorTest 17.1781%; camera correction alone 62.0891% and 49.3008%; declared lighting correction 96.9874% and 95.0759%. Percentages count pixels within eight codes among the union of lit pixels; the latter results still have 5988 and 9810 disagreeing pixels. SpecularTest improves from 14.2605% to 35.0852%, with 25346 pixels still apart. Logs `build/front-door-manifest-ortho.log` and `build/front-door-manifest-light.log`. Every result and the three reference PNGs were opened. Framing now aligns and the unilluminated right-hand cells are correctly dark; normal/specular differences remain. No conformance claim.

Confirmed remaining paths: `Live::StandsSubjects` copies native `Material` rows but does not bind their public maps. `Live::EmitsPerPart` uses emission or baseColour times indirect light even for `Unlit`; that contradicts the glTF unlit base-colour contract. The corpus also declares emission palette overrides with unlit=yes and baseColour=0, so both declarations and renderer semantics need a coordinated correction, with existing oracle comparisons retained. Public occlusion and normal scale/occlusion strength remain incomplete. The ellipsoidal/ASL datum mismatch is still unresolved.


### Unlit base colour, 2026-09-07 continuation

`Live::EmitsPerPart` now uses BaseColour for Unlit, ignoring emission and indirect light. This follows KHR_materials_unlit's baseColourFactor * baseColourTexture * vertexColour contract; alpha/double-sided handling is a separate remaining concern. The corpus palette translator now declares unlit base colours rather than the contradictory unlit+emission with black base colour. No oracle or threshold changed.

Negative control: with corrected declarations and old engine, UnlitTest is fully black, 0% agreement, 55517 pixels apart (`build/unlit-standard-before.log`, 16e6b295). Positive engine: UnlitTest100%, worst0 (8ef62b10); VertexColorTest99.9939% (e079f3a5), TransmissionTest99.9980% (8a6d1e53), both retain prior digests. SpecularTest stays35.0852%/25346pixels before the separate hemisphere correction below. All five negative/positive PNGs opened. Transmission remains an emission-palette geometry check, not transmission conformance. The UnlitTest uses palette overrides and therefore verifies public Unlit row shading, not complete original-asset material preservation.

Source: https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_materials_unlit/README.md


### Geographic light azimuth, 2026-09-07 continuation

Scenario::Light now explicitly declares angles toward the source, elevation above the horizon and azimuth clockwise from north (-Z in East-Up-South). Camera and sun use shared EastUpSouthDirection. The sun previously mirrored north/south while the camera was already correct. The thirteen-assertion cardinal/zenith regression fails two assertions under the original sign and passes after repair; conventions4/4. The corpus glTF beam inverse uses atan2(-direction.x, direction.z), preserving imported directions. Material comparison results remain unchanged and three remain red. Full render and visual evidence is recorded in board2167's geographic azimuth continuation; all nine places meet this static frame budget, none constitutes final visual acceptance. Native map binding, orthographic shading view direction, normal transformation under nonuniform scale and height datum remain unresolved public-contract issues.


### Native geometry and light placement, broader convention audit

User explicitly expanded the audit beyond camera conventions. Two more silent declaration losses are repaired at the public Geometry -> internal Subject boundary:

- Subject::AssemblePartInto ignored Geometry's part placement. It now applies the local-to-model affine transform to positions, transforms and normalizes supplied normals using cofactors with determinant sign (the normalized inverse-transpose direction), transforms tangents as surface directions, reverses tangent handedness under reflection, and swaps triangle corners for a negative determinant. Identity placement retains its original exact copy fast path. Missing normals/tangents are generated afterward from placed geometry. The existing Geometry::placementOf const getter is now public, matching the already-public lampPlacementOf; its contract is documented.
- Subject::AssembleLights overwrote a light's local position with placement translation and ignored beam orientation. It now transforms the local point and beam, normalizes the beam and leaves intensity/range unchanged. Subject::Handed now puts the light at local zero beside its placement translation, avoiding double translation on reassembly. Public PunctualLight documents the emitted-ray direction and zero-as-unbounded range.

Geometry negative control: NativePlacementPreservesTheSurface failed28/38 assertions before repair; after repair38/38 passed. Independent analytic fixture: scale(2,3,4) maps plane normal(1,0,1)/sqrt(2) to (2,0,1)/sqrt(5), tangent(1,0,-1)/sqrt(2) to (1,0,-2)/sqrt(5); translating by(5,7,11) and reflecting X test point positions, triangle winding and handedness. Logs build/native-placement-before.log, build/native-placement-negative-assertions.log, build/native-placement-after.log. Initial harness BUILD failure lacked -Isrc/base for Subject.h's math/Box.h; the conventions include path was fixed before capturing the actual failing assertions.

Light negative control: extended fixture failed8/59 assertions before light repair; after repair59/59 pass and suite5/5 (build/native-lights-before.log, build/native-lights-negative-assertions.log, build/native-lights-after.log). A rotated/scaled placement maps local point(1,2,3) to(17,13,9), beam-Z to-X, with intensity37 and range19 unchanged. Point/spot/directional types and Subject -> Geometry -> Subject round-trip checked. This is numeric boundary evidence, not a rendered photometric conformance claim. Standard reference: https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_lights_punctual/README.md (inherited orientation/position, scale does not alter intensity/range).

Render controls build/native-placement-places.log: Husumc98da96a p993.37ms, Malcesine56b80c58 p997.79ms, both0/120 over16.67ms. Both full PNGs reopened; digests unchanged from the previously inspected full/crop/webcam comparisons. Same unacceptable shoreline teeth and blue cliff curtains. Material controls build/native-placement-materials.log retain NormalTangent96.9874%/5988px239829a2, Mirror95.0759%/9810px67c5dfb9, Specular87.0995%/5037px9985b064; all three PNGs reopened. All three remain RED; no oracle changes.

Broader audit remains open. Confirmed next targets: litVertex.glsl transforms instance normals by the model matrix rather than inverse transpose and retains tangent.w under reflections; instance winding also needs review. subjectLighting uses normalize(-position) for orthographic views; spot angular falloff is linear rather than the squared curve in KHR_lights_punctual. Native Material texture binding and normalScale/occlusionStrength, alpha-mode preservation, ellipsoidal/ASL height conversion remain incomplete. Transform setters also accept matrices without checking the affine/finite contract. Naming/layout differences listed at the top are separate from these semantic defects. No claim that include/ or the renderer is generally conformant yet.


### Orthographic shading rays, 2026-09-07 continuation

The projection was orthographic but subjectLighting still used normalize(-position), a perspective view vector. FrameContext now carries a homogeneous camera-relative ViewPosition: (0,0,0,1) for perspective, (-cameraForward,0) for orthographic. The shared lighting shader evaluates normalize(view.xyz - position * view.w). This gives parallel rays for orthographic specular/Fresnel/anisotropy while retaining the camera-origin perspective expression. The frame context is208bytes; light uniforms have six vec4 header rows followed by16lights,1120bytes. A named kLightHeaderFloats=24 replaces the duplicated light-array offset. No material defaults or oracle changes.

Vendor image sequence, before -> after (pixels differing by more than8 codes): NormalTangent5988 ->2086, agreement96.9874% ->98.9505%, PNG239829a2 ->51a01e96; Mirror9810 ->1741,95.0759% ->99.1261%,67c5dfb9 ->f2a3cadf. Both positive PNGs opened. Perspective Specular remains87.0995%/5037px/9985b064. The three comparisons remain RED against99.99%; build/native-placement-materials.log is before, build/orthographic-lighting-after.log is after. Orthographic highlights now stay in the same place across equivalent cells, but edge/normal-map differences remain.

Actual GPU negative control: OrthographicLightingIsParallel creates a native lit plane through Engine/Geometry, reads linear float pixels for identical orthographic cameras at axial distances2m and8m. Old shader produces a maximum linear difference0.264038086, failing the1e-5 invariance threshold. Corrected shader passes; controls check nonempty images and actual illumination. Test6assertions, full conventions suite6/6. Logs build/orthographic-lighting-negative.log, build/orthographic-lighting-negative-assertions.log, build/orthographic-lighting-positive.log. The first fixture attempt was UNPREPARED because Sees.Placed was missing; that declaration was corrected before collecting the real negative. The temporary old shader expression is restored to the corrected homogeneous expression; no control active.

Perspective place controls expose existing nondeterminism rather than a clean digest attribution. First run build/orthographic-lighting-places.log: Husum366d2503p993.34ms; Malcesine58b0b22bp995.13ms. Compared with precedingc98da96a/56b80c58:465/1373 changed pixels,183/500 differ by more than1, worst25/23. Largest differences at terrain/class edges (Husum800,437; Malcesine245,342). Both full PNGs and enlarged shore/cliff crops build/orthographic-lighting/*-detail.png opened; no visual acceptance.

Repeat without source edits (both Make logs report zero stripped/rewritten files and no client relink) returns OLD digests: Husumc98da96ap993.37ms, Malcesine56b80c58p997.09ms (build/orthographic-lighting-repeat.log). All four rows0/120 over16.67ms. No claim that every perspective pixel is stably unchanged; the cause of these alternate terrain-edge pixels remains open and is recorded in board2154. This turn does not establish a new performance/visual acceptance for all nine places.


## Declared atmosphere and general conventions continuation, 2026-09-07

Live::DeclaredAir now supplies Hazed(kEarthAir, Declared_.Haze) to BOTH CPU SunThroughTheAir and Renderer::SetMedium. The air cache compares its actual float cosine and Medium; its old metre tolerance on a cosine is gone. SamePicture includes Haze so changed declarations invalidate the picture. ClockedExposureUsesDeclaredAir negative: Haze0/1/2 all85072.454561337lux, fails response assertion. Positive:84931.058194878 /85072.454561337 /85127.545410845lux,7assertions pass; conventions7/7. Logs declared-air-before/after and declared-air-negative-assertions. Increased aerosol does NOT simply darken total horizontal irradiance: scattered light redistribution is a counterexample. This is consistency evidence, not independent absolute photometry. Dynamic redeclaration invalidation not separately tested; ground albedo/observer height consistency still open.

All9 make shots terminal46251 exit0, build/declared-air-all-places.log. Name,digest,p99ms: DarmstadtWest,e3480f35,3.21; Wien,fca0b043,5.85; Rosenheim,94287023,4.75; Husum,84ab9c53,3.32; Olympiaturm,a95d6751,4.04; Graz,0ad02ce2,5.11; Koerbersee,552e2a97,9.28; Malcesine,90ecdd5f,4.72; Feldkirch,0ca91bb4,5.80. All0/120 over16.67ms. Earlier2Placeaudit44687: Husum3.51/Malcesine5.72ms same digests. Every all9 comparison PNG (render+webcam) AND every2x detail PNG in build/declared-air-review opened visually. Graz initially truncated tool result was reopened successfully. Rejected: Husum shoreteeth/ramp, Malcesine blue vertical curtains/horizontal band, Feldkirch nearly black abruptwall, Graz floating geometry and hill spatial mismatch. Koerbersee coarse ridges and abrupt material borders. Cities flat prisms/darkfacades and absentvegetation; allsky/weather mismatch. Performance is a pass, visual acceptance is NOT.

Materials terminal41748 exit2, build/declared-air-materials.log: Unlit100%8ef62b10; Normal98.9505%2086px51a01e96; Mirror99.1261%1741pxf2a3cadf; Spec87.0995%5037px9985b064. All4PNG reopened. Three remain RED against strict99.99%, no oracle change.

### Spot angular attenuation

Public native Spot light path rendered linear cone attenuation, inconsistent with Khronos's steep-then-level curve. subjectLighting.glsl now multiplies attenuation by angular*angular. Standard: https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_lights_punctual/README.md#inner-and-outer-cone-angles . The squared interpolation is the reference implementation, not the sole allowable curve.

NEW SpotFalloffMatchesKhronos.cpp renders three65x65 native lit planes via public Engine/Geometry: pointcontrol, spotcosine0.75 between innercos1/outercos0.5, spotaxis. Odd dimensions put center sample at planeorigin, equalposition/distance/BRDF cancels in ratio. Expected midpoint=((0.75-0.5)/(1-0.5))^2=0.25, axis1. Negative actualGPU0.5midpoint/1axis:1/15fails (build/spot-falloff-before.log, spot-falloff-negative-assertions.log). Aftershaderfix15/15pass, fullconventions8/8 (build/spot-falloff-after.log). Terminal4030negative exit2,75963positive exit0. No transient controls active. No further Places run after spotlight-only correction; all9above belongs to declaredair correction. No source/board edits during gates.

Remaining goal unchanged: native texture binding, normalScale/occlusionStrength, alpha preservation, instance normal/mirror and Standing.ScaleXyz consumer audit, vertical datum; directional sky/AO/localbounce; terrainDEM projected1px bound/crackfree/noSkirts/motion; all9visual acceptance. TerrainGrid::FlattenOutliers tilemedian/MAD may flatten realfeatures but this is UNPROVEN; actual69badHusumsamplespreviouslyverified, do not remove sanitization blindly. Materialcorpus3red; nondeterministicPlacesedgesopen board2154. No completion claim. Current turn made concrete progress and is NOT blocked.
