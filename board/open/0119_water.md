Type: feature
Area: render
Tags: scope

**II.4 Water**

- [x] Water polygons with a level per ring (`world/WaterField`)
- [x] Water surface tessellated at level + 0.15 m over a declared 24 B layout
- [x] Water lines with a monotone downstream gradient
- [x] Water depth at a point as a type that cannot be negative (`core/WaterDepth.h`)
- [x] Depth derived analytically from water level minus ground height — no blended fragment, no separate pass
- [ ] Level model that does not put nine of nine outlines under their own ground — the fifth percentile of a ring under 22 points *is* the minimum
- [ ] Body colour by depth with a declared extinction per wavelength
- [ ] Surface normal perturbation from a wind-driven wave function
- [ ] Fresnel reflection of the sky LUT
- [ ] Reflection of the shore — the reference uses a screen-space term; UNSURE which
- [ ] Refraction of the bed at shallow depth
- [ ] Caustics — the reference ships water-volume caustics from an authored texture; NO SUBSTITUTE is false here, a function reaches it, but nothing is built
- [ ] Foam at a shore line, driven by depth and slope
- [ ] Foam and turbulence at a weir or a rapid
- [ ] Flow direction and speed on a watercourse, visible in the surface
- [ ] Waterfall — the reference's river tool cannot make one either, and says so
- [ ] Shoreline wetting band, darker than the dry bank
- [ ] Floating debris, leaves, ice
- [ ] Ocean with a swell spectrum — out of scope for the acceptance place, named so it is not an oversight
- [ ] Boats displacing water and leaving a wake (band V depends on this)
- [ ] Rain rings on a still surface
- [ ] Underwater view: extinction, god rays, surface from below
