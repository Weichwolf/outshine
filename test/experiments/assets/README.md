# Synchronous asset references

`SynchronousAssetGeneration.h` retains the synchronous control used by offline
research probes. It is not part of the application or the runtime asset API.
Runtime package values live in `AssetPackage.h`; metadata lives in `AssetRecord.h`.

Build the current native probe consumers through CMake:

```sh
nice -n 10 cmake --build build --parallel --target \
  outshine_experiment_urban_bake outshine_experiment_plant_prepare \
  outshine_experiment_urban_control outshine_litter_page_probe
```

Reference-control checks: `outshine/integration/experiments/assets`.
Runtime checks: `outshine/src/content/assets` and the concrete prepared-generator
modules. Experimental success does not qualify runtime admission or performance.
