# Exact-time weather snapshot and low-cloud consumer

[#253](https://github.com/Weichwolf/outshine/issues/253) owns the confirmed
field/time/missing contract; [#9](https://github.com/Weichwolf/outshine/issues/9)
owns the public boundary. `world/weather/WeatherSnapshot.h` contains owned native
values and provenance, independent of Open-Meteo, JSON and forecast containers.
Unknown optionals stay unknown; known zero stays zero. Wind is horizontal ENU at
10 m AGL. Amounts retain their preceding-hour interval and physical units.
Requested location, returned model grid, downscaling height, validity, acquisition,
model initialization, source ID/revision and original-byte digest remain separate.

`Source::DecodeWeather(bytes, utcS)` runs on a compute worker, outside the frame.
It returns an owned exact-time snapshot, an empty optional for absent exact UTC,
or an unsupported/corrupt error. It never selects a neighboring hour, interpolates,
does IO or falls back to scenario defaults. Bounds remain 1 MiB and 4096 hours.
Each call decodes the bounded payload; no implicit decoded-state cache is added.

## Provider and source ownership

`generation/weather/OpenMeteoProvider.h` is the concrete extension factory.
The caller supplies typed original request coordinates and optional recorded
acquisition/model metadata. This trusted context must match the actual request;
it is not reconstructed from URI parameters. Registration uses the ordinary
`ProviderRegistry` / `Engine::registerProvider` contract. Returned sources copy
the complete context, so neither source nor snapshot borrows factory inputs.

Declare kind `open-meteo`, a dataset/revision, the exact original HTTPS endpoint
and its mandatory lowercase SHA-256 digest in `SourceProvider`. No location,
coverage chunk or alternative schema is accepted. This pinned point/capture
occupies `DataKind::Weather`, `WireFormat::ProviderDefined`, `Address::Whole(0)`.
It makes no claim about another position/address and performs no automatic refresh.

The source uses existing `SourceSet`, `Transport` and `ContentStore` ownership,
cache identity, payload cap and cancellation. A changed revision/endpoint/digest
has a different existing cache key. HTTP absence/failure remains explicit;
invalid original bytes cannot publish a snapshot. Partial byte ranges are refused.
There is no second IO worker, cache implementation or product-type registry.
Forecast decoding and normalization remain in `generators/weather`.

## Actual consumer and explicit procedural completion

`generators/weather/LowCloud` consumes the snapshot's **low** cloud coverage and
produces the existing native `CloudLayer`, used by `CloudOpticalDepth`.
Known low cover zero gives exactly transparent cloud extinction, even when high
and total cloud cover are nonzero. Missing low cover yields no low-cloud state.
This correct treatment of the captured Koerbersee forecast is the first source case.

Positive low cover requires an explicit `LowCloudRecipe`: ASL base, thickness,
extinction and horizontal ENU advection. Geometry/advection are marked procedural
in the product and never written into the source snapshot. The recipe is not
measured cloud geometry; 10 m wind is not silently promoted to cloud-height wind.
`RenderFrame` alone converts ENU advection into the existing density field's frame.
A separately declared snapshot uses `ScenarioDeclaration` provenance; unavailable
forecast values never create this case implicitly. Existing scenario declarations
and their validation/import/export remain unchanged.

## Reproduction, checks and remaining integration

Build `weather_snapshot.cpp` against the **matching rebuilt** `build/liboutshine.a`
with the project's C++23 toolchain. Include `include`, `src/base`, `src/base/io`,
`src/base/format`, `src/world/data`, `src/world/weather`, `src/generators/weather`
and `test/harness/shared`. The probe uses no GPU or engine/frame calls.

```sh
snapshot_out="${TMPDIR:-/tmp}/outshine-weather-snapshot-$(git rev-parse --short HEAD)"
mkdir -p "$snapshot_out"
# Place the matching compiled probe at "$snapshot_out/weather_snapshot".
python3 -W error test/experiments/weather/weather_snapshot_controls.py \
  --probe "$snapshot_out/weather_snapshot" --output "$snapshot_out/controls"
sh test/run.sh outshine/src/generators/weather/OpenMeteoSource
sh test/run.sh outshine/src/generators/weather/OpenMeteo
sh test/run.sh outshine/src/world/weather/CloudLayer
```

The executable actually consumes the source snapshot through the native low-cloud
optical-depth kernel. Its controls first prepare the original capture through the
ordinary SourceSet/Transport/cache path, then launch 24 fresh offline processes
from a receipt-only directory without the original payload. They verify exact UTC,
source digest, known-zero extinction, one cache hit and zero provider starts each.
Absent/invalid UTC and an empty offline cache stay explicit failures. Unit tests
also cover positive declared cloud, recipe errors, provenance, retained unknowns,
digest rejection and ownership after factory/source destruction. Inputs are the
unchanged versioned [capture response and receipt](data/open-meteo/README.md).

This is native state/consumer evidence, not a Place image, full-world loading or
GPU-budget claim. Productive SceneRenderer/Stages wiring stays with #328; common
light/shadow composition with Light. Material needs a later separate integrated
wetness/snow history, not direct rain/snow-depth parameters.
