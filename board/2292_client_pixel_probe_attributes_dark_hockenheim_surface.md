Type: diagnostic
State: done
Architecture: ready
Parent: 2167
Depends: 2291, 2293
Priority: P0
Area: client, render
Tags: lighting, hockenheim, pixel, attribution

# Client pixel probe attributes a dark Hockenheim surface

## Problem and evidence

At Hockenheim lap station 1373.068 m (74.850 s), the paced trace reports
`Refined`, while separate still captures stopped at `Playable` and alternated
foreground RGB (4,10,26)/(91,88,83) as the world and shadow atlas changed.
Colour alone cannot distinguish shadow, material or transient geometry.

## Contract and ownership

`src/client` owns a bounded `run --quality refined --probe-pixel x,y` diagnostic using only the
public `Renderer::readPixels` API after a Refined final frame. It reports
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
- Probe mark 3 at foreground/road/horizon and mark 4 at comparable pixels,
  requiring `settled(Refined)` for each still before comparing colour.
  If identity and normal agree while colour diverges, investigate direct vs
  indirect illumination next; if identity differs, trace the covering product.
  Do not claim attribution from similar RGB alone.
- `run --help` gives the exact option and row schema. Focused CLI/Engine tests,
  `make format`, `LINT_JOBS=2 make lint` pass. Board 2167/2260 records the
  observed cause and next corrective work, not a picture-specific workaround.

## Result

`run`/`measures --quality refined --probe-pixel x,y` retain two optional native attachments and
read one final pixel through the public renderer. Parsing rejects malformed and
overflowed coordinates before SDL; an out-of-target coordinate fails before
assembly. The TSV includes RGBA8, linear RGB, depth, normal, surface ID and
quality. Static diagnostics wait for Refined; paced motion probes its final
frame. Screenshot and probed RGBA agree. Mark-3 road/horizon and mark-4 road/
sky were inspected. At mark 3, same ID/normal/depth but dark paced vs bright
static colour proves a render-state discrepancy, not a contact
gap. Mark 4 exposed a rare overhead-geometry anomaly. WI 2295 owns both
remaining render defects. Focused tests, format and full lint pass.
