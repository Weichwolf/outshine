Type: feature
State: active
Architecture: ready
Priority: P1
Parent: 2171
Depends:
Area: render, materials

# Rock surfaces have filtered world-space structure

## Evidence and boundary

Malcesine at 6761d703a: final z13 DEM rises 321.28 m along a 100 m camera-ray
segment, and the final height page agrees within 1 m at both ends. Current
`groundLit.glsl` only blends one rock albedo/roughness row into the slope; the
exposed cliff remains a uniform gray curtain. The final geometric relief and
silhouette still belong to WI 2166. This unit improves the independent PBR
surface response and must not pretend shader bump fixes geometry.

## Decision

- Keep one material path: `groundClass` selects a template and its existing rock
  slope blend; `groundLit` feeds filtered albedo, roughness and normal into the
  same Metallic-Roughness BRDF. No rock-only draw pipeline or imported texture.
- `slope.plausibleDeg` currently doubles as rock-exposure permission: asphalt and
  concrete at 10° wrongly become rock on steep constructed ground. Add explicit
  material data `slopeExposesRock` (missing = false) and pass a 90° no-exposure
  threshold to the shader for classes where it is false. Keep plausible slope
  intact for material/terrain validation.
- Use stable tangent-world metres, including elevation, for a non-periodic 3D
  procedural field. Carry the ground world's height explicitly from the lattice
  vertex stage; do not use camera-relative `position` as a noise seed. Shared
  positions must agree across tile seams and remain stable during camera motion.
- Pack the template's existing `GroundSurf`/`Mix` parameters through the palette
  instead of inventing one shader style for every class. Macro structure may be
  artistically set in versioned material data; validate units and limits. Distinct
  wavelengths control broad color variation and close normal/roughness detail.
- Fade octaves by fragment footprint before Nyquist. Bump changes the shading
  normal only; amplitude and material albedo stay bounded and dielectric.
  Constructed terrain classes with disabled rock exposure remain unchanged.
- Check stage bindings, world-origin moves and shader cost. A measurable regression
  or visual repetition is a failure, even if a static screenshot is prettier.

## Acceptance

- Malcesine/Koerbersee screenshots are visually inspected against saved PNGs;
  natural cliffs read as structured rock at near/mid distance, sky/water and
  manufactured surfaces remain clean. Record changed pixel area and frame p99.
- Two nearby camera positions and at least one distant view show no crawling,
  tile boundary or unfiltered high-frequency pattern. Suppressing the rock weight
  removes the effect; steep manufactured class remains visually unchanged.
- `make format`, focused shader/client cases, and `make lint` with zero tidy
  findings. Do not close WI 2166 or parent 2171 on this shading slice.
