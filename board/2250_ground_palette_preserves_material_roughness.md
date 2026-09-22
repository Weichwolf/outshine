Type: defect
State: active
Architecture: ready
Priority: P1
Parent: 2171
Depends:
Area: engine, render

# Ground palette preserves material roughness

## Evidence

`VegetationTemplates::ReadSubstrate` writes material roughness into `Row::Ground[3]`.
`Engine::State::PaletteOver` replaces that channel with `Mix[2]` (specular scale).
`groundLit.glsl` ignores that fourth channel and shades every ground class with
the shared material roughness. It also passes zero dielectric F0/F90 to `shadeRow`,
removing the dielectric specular lobe. The declared material is not rendered.
This predates the last twelve hours and is independent of streaming provenance.

## Decision and owners

- Preserve linear RGB + roughness in each palette row in `src/engine/Laying.cpp`.
  Keep the existing storage layout. Use native default material roughness for
  the unclassified fallback, matching `BeginsGroundSurface`.
- `groundWearsSlope` and class-edge interpolation already mix all four channels.
  Feed the resulting roughness to `shadeRow` in `groundLit.glsl`, together with
  `surface.f0` and `surface.specularWeight` as in the native `shade` path.
  Keep metalness and all other material/light parameters intact.
- Do not tune catalogue values to hide geometry faults or add per-Place settings.
  No new material model, texture cache or speculative module extraction.

## Acceptance

1. Exercise low/high roughness with equal colour, geometry and lighting. Prove
   catalogue value reaches palette and ground BRDF, including fallback/mixing.
   A constant roughness mutation must fail. Reuse existing renderer fixtures;
   no second client or shader implementation.
2. `make format`; targeted material/renderer cases; `make lint`.
3. `make shots PLACE=Malcesine`; preserve/open before/after PNGs and compare using
   `test/scripts/pixels.py`. Baseline at 21342822f: Malcesine-762c673c. Same CPU
   geometry; classify changes as material response. Folds, flat water and fallback
   structure capture variation remain open.
4. Report GPU/CPU timing separately. A nearly unchanged distant diffuse image
   does not invalidate a correct roughness fix or prove overall visual quality.
