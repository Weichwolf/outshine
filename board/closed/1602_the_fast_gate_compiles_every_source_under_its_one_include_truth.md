Type: issue
Area: test
Tags: build

**The fast gate compiles every source under its one include truth**

board:1601 made `test/run.sh` without arguments the regression gate — but the gate only
compiles what its kept suites' `LayerGroups` name, and those lists leave whole layers dark:

- `src/sim/Journey.cpp` is compiled by NO fast-gate suite (`unit/sim` groups only
  `src/scenario/ScenarioRead.cpp src/sim/Rigging.cpp`, test/run.sh:146; every group carrying
  `src/sim` entire — `tools/driver*` — is named-only). The file this hour's work centred on
  can stop compiling and the gate stays green.
- `src/world` is proved by ONE of its ~15 units (`unit/world` groups
  `src/corridor src/world/Wayfinding.cpp`, test/run.sh:157). GroundStream, OsmField,
  RoadHarvest, the fields: all named-only.
- `src/clients/{Sim,LogSinks,StreamTelemetry,EyeTelemetry,CsvTelemetry,Species,RegionForge,
  SceneWeather}.cpp` live only in `render/outshine/world` (named-only).

Consequences beside the hole:

- The mirror claim (`test/harness/claims/EverySourceLayerHasItsUnitMirror.cpp`) proves
  DIRECTORY existence (`HoldsTests`), not that the suite's groups cover the layer's sources —
  `unit/world` satisfies it while compiling one file. The claim is satisfiable by a hollow
  mirror.
- `NAMED_ONLY` (test/run.sh:443) spells `render/outshine/grown`, a suite that does not exist:
  the harness is `harness/render/outshine/grown`, already excluded by `harness/render`, and
  `test/render/outshine/grown` holds manifests, no `.cpp`. A declared list with a dead entry.
- `Makefile` still documents `make test` as "run every test" while run.sh's default is now the
  fast gate with loud exclusions. One of the two lies.
- `kFastGateBoundMs=90000` was [SET] against a warm 39.6 s baseline that the same hour grew to
  45.8 s; the elapsed clock also counts compile time, so a cold `$TMPDIR` (CI, fresh machine)
  reds a healthy tree on wall time alone.

Demanded: the gate opens with `BuildLibrary` (incremental — `UpToDate` makes the warm cost
seconds), so EVERY source compiles under its one declared include set on every gate run;
re-measure the bound over that population and name the warm/cold populations apart; delete the
dead `NAMED_ONLY` entry; make the Makefile's `test` comment tell the truth.
