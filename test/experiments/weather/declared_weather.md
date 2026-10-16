# Explicit native scenario weather

The contract, ownership and acceptance belong to
[#421](https://github.com/Weichwolf/outshine/issues/421).
The public types document native units and missing-value semantics in
`include/scenario/WeatherDeclaration.h` and `include/world/weather/WeatherSnapshot.h`.
The fixed input is `test/outshine/data/weather/declared-overcast.scenario`;
its root `<weather>` section is independent of legacy `<world><weather>` metadata.

After a complete regular `make all db` with the project's dependency/toolchain setup:

```sh
python3 test/experiments/weather/declared_weather_native.py \
  --archive-root "$PWD" --output /tmp/outshine-weather-declared-native \
  --width 1280 --height 720 --measure-cost
python3 test/experiments/weather/declared_weather_report.py \
  --captures /tmp/outshine-weather-declared-native \
  --output /tmp/outshine-weather-declared-report
```

The actual production parser and public engine supply four native raster cases:
low cover0.65, low cover1, known0 and missing low cover. Other declared fields remain
unchanged. Each case captures two identical Vulkan frames with validation enabled.
The PDF uses the same controlled air, light, geometry and phase37s; geometry and cloud
advection are explicit procedural recipes. This is an initial-state consumer proof,
not a productive Place, complete weather, lighting, frame-budget or device acceptance.
The independent FP64 reference checks every pixel in bounded batches and verifies
that the actual GPU frame consumes the declared cover. Missing has no cloud product;
its matching clear image does not turn it into an observation of clear weather.

The optional CPU probe reports1000 iterations per case. For allocation/heap-traffic
evidence, run Valgrind DHAT separately on `declared-weather-cost` with an explicit
temporary `--dhat-out-file`. Instrumented times are not native timings. A fresh-process
RSS peak includes libraries and all cases. Snapshot reuse does not make the entire
existing `Engine::assemble` path allocation-free. Receipts retain full commands,
producer commit, archive/binary/shader hashes and original scenario inputs.
