# Engine tests

| Directory | Responsibility |
|---|---|
| `include/<header without .h>/` | Existing header contracts during the module-header migration in #596 |
| `src/<component without .h or .cpp>/` | Implementation contracts; mirrors the component under `src/` |
| `integration/places/` | Complete offline places at 1280×720/30, CPU/GPU and image checks |
| `integration/places60/` | Separate 1280×720/60 comparison with the same contents and checks |

Examples: `src/content/assets/GeometryAsset/`, `src/base/curve/Ribbon/`,
`src/render/Shape/`. Name each case after the behavior it proves.
Test helpers belong with their component or in `test/harness/shared/`.
Keep one owner for each behavior; extend its cases instead of repeating the same setup
and assertions in a new executable. Keep API validation, asset persistence and GPU image
response separate: success at one boundary does not establish the others.

The default `make test` selects the world contracts in `test/world-gate.txt` and renders
Wien, Tokyo and Central Park offline at 1280×720/30. `make test-all` runs every declared suite.
`make lint` retains shared shader/GPU contracts independently of compatibility images.

Run a subtree or a single case. `suite` builds the selected component closure,
without building the complete application. Rendering profiles prepare their shaders.

```
make suite SUITE=outshine/src/base/curve
make suite SUITE=outshine/src/content/assets/GeometryAsset/MaterialTablesRemainCompleteWithoutMeshes
make suite SUITE=outshine/integration/places
make suite SUITE=outshine/integration/places60
```

Choose checks by the changed boundary:

| Change | Focused check |
|---|---|
| Base algorithms, IO, tasks | `make suite SUITE=outshine/src/base` or the affected subtree |
| Assets, codecs, materials, animation data | `make suite SUITE=outshine/src/content` or the affected subtree |
| World data, terrain, weather and entity storage | `make suite SUITE=outshine/src/world` or the affected subtree |
| HTTP acquisition and source integration | `make suite SUITE=outshine/src/host` |
| Scenario parsing and layering | `make suite SUITE=outshine/src/scenario/ScenarioRead` or `outshine/src/scenario/ScenarioLayer` |
| CPU render-geometry preparation | `make suite SUITE=outshine/src/render/Shape` |
| Shader file framing | `make suite SUITE=outshine/src/render/device/SpirvFile` |
| Species files and numeric definitions | `make suite SUITE=outshine/src/generators/flora/Species` or `outshine/src/generators/flora/TreeSpecies` |
| Engine file/publication integration | `make suite SUITE=outshine/src/engine/ScenarioFiles` |
| Memory and decoder boundaries | `make test-sanitizers SUITE=outshine/src/content/assets/GeometryAsset` |
| Concurrent execution and ownership | `make test-races SUITE=outshine/src/base/io/Tasks` |
| Renderer or shared GPU contracts | Affected render suites, then affected Places |
| Shared integration | `make test`; `make test-all` explicitly includes compatibility corpora |

Both instrumented commands require an explicit `SUITE`. Address/undefined behavior
and thread sanitizers run separately and instrument product objects as well as cases.
Each selected case also runs normally and with standard-library container checks;
instrumented builds do not establish release performance budgets.

`test/run.sh` selects build profiles by component. Vector decoding/storage keeps
ASan/UBSan; GPU submission/storage keeps native GPU validation; process-allocation
checks link diagnostic instrumentation. Shared library objects retain their own
declared dependencies.
Base, content, world, host, scenario parsing and CPU shape preparation do not link the engine
or generators. Engine persistence and producer-integration assertions live in
their engine suites; primitive file/XML checks remain independently runnable.
Builtin-provider wiring belongs to `src/generators/SourceConfiguration/` and
`src/generators/osm/SourceConfiguration/`; generic source contracts stay under world.
The optional `OUTSHINE_GLO30_RASTER_PROBE` live-source check runs with
`src/host/Fetching/ByteRangesKeepObjectIdentity`; raster decoding remains offline.

Source-closure audits inspect each distinct resolved source-group list once per
run. Test discovery and execution still visit every case and instrumentation arm.
