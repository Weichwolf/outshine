# Engine tests

| Directory | Responsibility |
|---|---|
| `include/<header without .h>/` | Public API contracts; only public engine headers |
| `src/<component without .h or .cpp>/` | Implementation contracts; mirrors the component under `src/` |
| `integration/places/` | Complete place scenarios and rendered-image checks |

Examples: `include/scene/Geometry/`, `src/base/curve/Ribbon/`,
`src/import/Subject/`. Name each case after the behavior it proves.
Test helpers belong with their component or in `test/harness/shared/`.

Run a subtree or a single case:

```
make suite SUITE=outshine/src/base/curve
make suite SUITE=outshine/include/scene/Geometry/PlacementReplacementPreservesGeometry
make suite SUITE=outshine/integration/places
```

`test/run.sh` selects build profiles by component. Vector decoding/storage keeps
ASan/UBSan; GPU submission/storage keeps SDL validation; process-allocation
checks link diagnostic instrumentation. Public API tests compile without `src/`
include paths. Shared library objects retain their own declared dependencies.

Source-closure audits inspect each distinct resolved source-group list once per
run. Test discovery and execution still visit every case and instrumentation arm.
