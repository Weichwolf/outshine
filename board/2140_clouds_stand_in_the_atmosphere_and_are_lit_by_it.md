Type: feature
State: open
Architecture: planned
Priority: P1
Parent: 2169
Area: world, generators, render
Tags: clouds, weather, lighting, budget
Depends: 2172

# Weather generates clouds that share atmosphere and ground lighting

## Current evidence and importance

SkyStage renders atmosphere LUTs; source audit finds no cloud renderer. Scenario::Weather
has validated/serialized cloud fractions, base AGL and wind, but these have no cloud/wind
consumer. World WeatherProvider/CalmWeather are not evidence of rendered weather.
Sky occupies roughly 1/3–2/3 of typical views [user design target]. Cloud form, illumination
and motion are core image quality, with ground-light effects even when sky is occluded.
P1 beside buildings/forest, rather than a late optional effect or a private time allowance.

## Binding architecture

- world/weather owns coherent validated time/weather snapshots and source provenance.
  Generators own deterministic density fields; render owns integration/history/quality.
  Use one frame origin, sun energy and snapshot for sky, clouds, shadows and sky irradiance.
  No renderer generator callbacks, separate cloud weather clock or Place-specific noise.
- Begin with ONE bounded volume layer and a declared isolated overcast/cumulus scene.
  Use CloudCover/base AGL/wind from the existing scenario boundary and an externalized
  plausible layer profile. This does not claim support for all ambiguous layer requests.
- Reuse atmosphere transmittance/radiance, native textures and render pass graph. A cloud
  pass produces radiance/transmittance; composition applies atmospheric transport once.
  Shared density drives cloud-shadow transmittance and diffuse sky response on ground.
- Reduced-resolution bounded ray marching is the first experiment: 320x180 versus
  1280x720 gives (320/1280)*(180/720)=1/16 pixels, not 1/4. Try 48/64/96 steps [SET];
  measure empty-space skipping/early-out before adding hierarchy or advanced scattering.
- Bounded temporal history rejects depth/transmittance disocclusion and resets after
  source changes/teleport. World-anchored field/wind advection is independent of camera.
  Fine cloud edges and temporal stability matter as much as still-frame smoothness.
- Quality levels consume 2314's COMMON scene budget with city/forest. No promised 3 ms
  or PS4-derived timing. Measure march, shadow, history, noise and composition costs.

## Open contracts before ready

- Scenario total/layer fractions are independently declared; altitude bands/overlap are
  unspecified. Bind requested versus generated coverage and conflicting inputs before
  supporting multi-layer weather; preserve parsed values and document actual capability.
- Bind density/height units, AGL terrain reference, origin shifts, texture bounds,
  optical coefficients and exact ownership/lifetime in the first implementation WI.
  2172's snapshot slice supplies the shared input; its complete wet-surface/weather work
  and 2213's finished moon/stars do not block an isolated sun-lit cloud prototype.
- Owners: world/weather snapshot, generators/atmosphere density products, render cloud
  stage and plan/Compiled composition, SkyStage/IrradianceStage/LightVisibility inputs.
  Respect reaches: render cannot consume generator implementation types.

## Independent acceptance

- Homogeneous slab has analytic T=exp(-sigma*length); zero density/cover is neutral.
  Wrong extinction, double atmosphere or detached ground transmittance FAIL controls.
- Clear, overcast, broken cumulus and mountain-intersecting low cloud: opened HDR/AOV/PNG
  evidence for edge/base/volume light, corresponding direct/diffuse ground illumination.
- Camera motion/teleport, wind turn and weather change: no permanent ghosting or swimming;
  source/history revisions, bounded work and overflow/refusal keep the last valid image.
- Native near/cloud quality ladders, city/forest mixed traces, p50/p95/p99, GPU capture,
  scratch/history/noise bytes and visual loss reported separately. No missing-cloud win.
- make format; density/transport/history/composition suites with independent controls;
  full lint. Render through outshine-client, open PNGs. 2092 owns target-device cost proof.

Local-reference investigation may compare existing Hillaire/Bruneton/Nubis material after
consulting pinned clones. A new renderer technique needs a measured advantage, not a name.

## Sichtbarkeit und Passgrenze

SkyStage zeichnet derzeit fullscreen ohne Depth-Test; Bildanteil und tatsächlich
bezahlte Arbeit sind daher nicht identisch. Einen späteren Sky-/Depth-Pfad erst nach
GPU-Messung umordnen. Cloudmarch benötigt opaque scene depth: Rayende an Oberfläche,
leere/verdeckt liegende Intervalle überspringen, Vordergrundnebel und Bergkontakt erhalten.
Cloudshadow/Irradiance bleiben auch bei verdecktem Himmel wirksam und budgetiert.
History/Upsampling achtet auf dünne Äste, Dächer und Disocclusion; Alpha-Geometrie darf
keine falsche opaque Raygrenze erzeugen. Komposition/Pass-Reihenfolge vor ready festlegen.

## Archivziel für die erste Lieferung

Rosenheim 15.07.2025 zeigt helle gebrochene Wolken mit dunkleren Basen; Koerbersee
15.07.2025 zeigt Wolken zwischen Kamera und Bergen. Eine reine Himmelstextur genügt
daher nicht. Zuerst bedeckt/gebrochen plus Bergkontakt mit derselben Dichte für
Transmittanz und Bodenlicht liefern. Exakte fotografierte Wolkenform ist kein Ziel.
