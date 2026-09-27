Type: defect
State: active
Architecture: ready
Parent: 2218
Depends: 2300
Priority: P0
Area: test, client, render
Tags: khronos, oracle, lighting

# Vendor relations use scene-linear client readback

## Evidence

`PointLightIntensityTest` declares five `statedInvariants` over translated
panels and explicitly says its exact appearance depends on IBL, exposure and
tone mapping. `render_corpus.py` ignores those declarations and scores every
case by full-frame PNG equality. Current Outshine PNG passes the interior
red/green/blue-to-white and RGB-to-white channel comparisons within 1/255,
but the generic score calls the case 20.2521% agreement. PNG quantization
cannot prove the manifest's 32-f32-ULP scene-linear bounds.

## Contract and ownership

`outshine-client run` exports a requested scene-linear RGBA32F attachment
through public `Renderer::readPixels(Buffer::Linear)` after the same captured
frame. A self-describing NumPy v1 file is a diagnostic product, never an
alternate renderer. Invalid/unavailable attachments and file failures refuse
the run. The normal path makes no float readback or file.

`test/scripts/render_corpus.py` dispatches on declared acceptance: a case
with `statedInvariants` evaluates those relations on the linear file; other
cases retain their image oracle. Region rectangles, channels, scale and
currency come from the manifest, with strict bounds/schema checks. The PNG
comparison remains a reported diagnostic for relational cases, not their
acceptance. No case name or source-path branch in the evaluator.

## Falsifiable acceptance

- All five declared PointLight relations are evaluated on one captured linear
  frame with the manifest's f32-ULP bound; no empty/black output can pass.
- Mutating one panel, one channel, one rectangle, the scale or the output
  extent makes the corresponding relation fail. Missing linear readback fails.
- `DirectionalLight` and other image-criterion cases retain their image gate.
- Focused client/harness tests, `make format` and `LINT_JOBS=2 make lint` pass.
