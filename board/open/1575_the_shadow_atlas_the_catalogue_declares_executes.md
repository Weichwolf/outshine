Type: task
Parent: 0120
Area: render
Tags: perf

**The shadow atlas the catalogue declares executes, and the ray demotes to the oracle suites**

The only executed shadow path is a per-pixel software BVH ray per light
(`ShadowRay.h:25-41`, called at `SubjectDraw.cpp:485`) -- the correct instrument against the
Cycles oracle, the wrong mechanism at 60 Hz: cost scales with lights x pixels x depth complexity
and caches nothing across frames. `LightVisibility` -> `ShadowAtlas` sit in the catalogue
(`RenderCatalogue.h:234-235`) and `Renderer::Executable` refuses them.

What ships: cascaded/atlas maps rendered only when a light or the world under it moves, O(1)
per pixel (id Tech 7 caches essentially all static shadows). The catalogue already names it.

- [ ] `LightVisibility` renders the atlas; geometry stages sample it
- [ ] the BVH ray remains for the render suites, selected by the declaration
- [ ] the sun's cast shadow lands in the driver's picture (the reviewer's standing finding)
