Type: diagnostic
State: active
Architecture: ready
Parent: 2167
Depends: 2291
Priority: P0
Area: client, render
Tags: lighting, hockenheim, pixel, attribution

# Client pixel probe attributes a dark Hockenheim surface

## Problem and evidence

At Hockenheim lap station 1373.068 m (74.850 s), `Refined` is ready and the
published road has centre and near-edge contact, yet the foreground reads
RGB (4,10,26) rather than the ordinary asphalt near (93,90,85). A separate
still capture reproduces it. The shape resembles a large cast shadow, but
colour alone cannot distinguish shadow, wrong material, occluding geometry or
unlit pixels. Blindly raising ambient light would conceal the cause.

## Contract and ownership

`src/client` owns a bounded `run --probe-pixel x,y` diagnostic using only the
public `Renderer::readPixels` API after the requested final frame. It reports
display RGBA8, scene-linear RGB, raw device depth, shading normal and surface
identity at one pixel. The option requires `--view --at-seconds`; nonnegative
integer coordinates parse before platform setup and are bounds-checked against
the actual target. A missing attachment or readback is an explicit error.
Neither the engine nor shaders gain a place-specific path. No readback occurs
without the option or inside the paced motion loop. Output is one documented
machine-readable TSV row with named columns and physical/encoding semantics.

## Falsifiable acceptance

- Invalid, overflowing and outside-target coordinates fail without a frame;
  a normal capture's PNG and pixel probe match its display RGBA at selected
  positions. Linear/normal/identity/depth sizes and finite values are checked.
- Probe mark 3 at foreground/road/horizon and mark 4 at comparable pixels.
  If identity and normal agree while colour diverges, investigate direct vs
  indirect illumination next; if identity differs, trace the covering product.
  Do not claim attribution from similar RGB alone.
- `run --help` gives the exact option and row schema. Focused CLI/Engine tests,
  `make format`, `LINT_JOBS=2 make lint` pass. Board 2167/2260 records the
  observed cause and next corrective work, not a picture-specific workaround.
