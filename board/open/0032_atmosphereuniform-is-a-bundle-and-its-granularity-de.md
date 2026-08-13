Type: bug
Area: render
Tags: scope

**`AtmosphereUniform` is a bundle, and its granularity defeats the dependency the graph already states — **Band 2****

`src/render/plan/RenderCatalogue.h:214-216` declares `AtmosphereUniform` as one `Given` resource; the
pre-port writer describes its contents in its own words — *"12 vec4: camera and sun basis, moon
direction, sky extra, view"* (`Renderer.cpp:229` at `235a7ff`). **Camera, sun, moon and view are four
quantities with four different rates in one resource.**

**Why it matters is not size, it is that the read set is the engine's only statement of what an output
depends on.** The graph already distinguishes correctly at the two extremes: `Transmittance` reads
**`{kNoEdge}` — nothing at all**, so its output cannot change and it need never be rebuilt; `SkyView`
reads `AtmosphereUniform`. **But `AtmosphereUniform` changes every frame because the camera is in it**,
so *"rebuild when an input changed"* saves nothing for any stage that reads it — the dependency is
finer than the resource that carries it.

**The harmless explanation, sought.** *A uniform is cheap and splitting it costs a binding* — true, and
irrelevant: the cost here is not the upload, it is that **a rebuild rule cannot be stated** while the
resource is a bundle. *It is only the atmosphere* — no: `CascadeUniform` will have the same shape the
moment shadows return, and the rule is general.

**Right:** `AtmosphereUniform` splits into **medium · sun · view**, each a resource, so a row's read set
says which rates its output actually follows. **Fixed when** `Transmittance`, `SkyView` and `Irradiance`
have read sets that differ, and a rebuild rule over them is expressible without naming a stage.
