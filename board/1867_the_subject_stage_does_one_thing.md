Type: issue
State: open
Area: render
Tags: architecture, stage, measured

# The subject stage does one thing, and the other five move out

`SubjectDraw` is the largest red on both CURRENT maps. Six responsibilities in one class,
beside the one `Encode` a stage owes:

| responsibility | at HEAD |
|---|---|
| shader source | `ShaderSource(const SourceOptions &options)` — SubjectDraw.h:30 |
| pipeline table | `PipelineAt(VertexLayout layout, SurfaceKind kind, bool cullsBack)` — :154 |
| upload / staging | `FlushCrossings(SDL_GPUCommandBuffer *commands)` — :148 |
| placements | `SetPlacements(const double *models, size_t rows, std::string &error)` — :51 |
| lights | `SetLights(std::span<const SubjectLight> lights, std::string &error)` — :89 |
| a second encode | `EncodeDepthOnly(const double lightFromWorld16[16], ...)` — :96 |
| **the stage's own** | `void Encode(const FrameContext &ctx, const PassRecording &into)` — :93 |

`SubjectResidency` already stands green beside it, which proves the split is available and
was only ever half taken.

## What will be true

- [ ] The stage encodes and nothing else: source, pipelines, residency and the light table are
      their own units, each reachable by a unit twin that does not stand up a device.
- [ ] `EncodeDepthOnly` is the shadow stage's own encode (board:1575), not a second entry point
      on the colour stage.
- [ ] Transmissive draws are a batch partition of this one stage, so the cloned
      `subjectsTransmissive` row (RenderCatalogue.h:268) disappears (board:1574).
- [ ] Proving test: `test/unit/render/stages/` holds one twin per new unit; negative control —
      the encode handed an empty residency draws nothing and says so.
