# Native hourly Open-Meteo normalization

[#253](https://github.com/Weichwolf/outshine/issues/253) owns the fields and
consumer agreement. `src/generators/weather` now decodes the same fixed response
as the [Python reference](data/open-meteo/README.md), using the existing `Json`
and SHA-256 implementation. The forecast remains private to the extension;
the public snapshot publication hook under #9 and its runtime consumer are open.

The acquisition caller supplies the original request URI, typed requested
position, HTTP status, payload digest and optional acquisition/model metadata.
That trusted context must describe the actual acquisition. The decoder checks
source identity, coordinate bounds and the digest, but does not reconstruct or
cross-check URI query parameters. The experiment derives this context from the
versioned acquisition receipt. Its microsecond timestamp remains in the receipt;
the native acquisition time is explicitly whole UTC seconds.

Each decoded hour owns optional native values: missing/null remains unknown and
known zero remains zero. Requested position, model-grid position and downscaling
surface elevation stay separate. Wind is horizontal at 10 m AGL, expressed east
and south; it does not assert cloud-height wind. Precipitation amounts belong to
the preceding hour. Cloud base, vertical wind and model-run UTC are not inferred.
No surface history, snow mass, cloud deck or current weather is invented.

`ExactHour` borrows an existing record by signed 64-bit UTC, without allocation,
interpolation or extrapolation. Its input must retain the decoded ordered hours;
the pointer lasts until its owning forecast is changed or destroyed. The decoder
does no IO and caps payloads at 1 MiB and forecasts at 4096 hours. Parsing, hashing,
source copying and hourly allocation belong to acquisition work, outside the frame.

## Reproduction

Requires a C++23 compiler/standard library with `std::expected` and `std::print`,
and Python 3.11 or newer. Use the same compiler/library pair as the project.

```sh
forecast_out="${TMPDIR:-/tmp}/outshine-weather-native-$(git rev-parse --short HEAD)"
mkdir -p "$forecast_out"
nice -n 10 "${CXX:-c++}" -std=c++23 -fno-exceptions -O2 -ffp-contract=off \
  -Wall -Wextra -Wpedantic -Wshadow -Werror \
  -Iinclude -Isrc/base -Isrc/generators/weather \
  test/experiments/weather/native_forecast.cpp \
  src/generators/weather/OpenMeteo.cpp src/generators/weather/OpenMeteoFields.cpp \
  src/base/format/Json.cpp src/base/format/Sha256.cpp \
  -o "$forecast_out/native-forecast"
PYTHONDONTWRITEBYTECODE=1 nice -n 10 python3 -W error \
  test/experiments/weather/native_forecast_controls.py \
  --probe "$forecast_out/native-forecast" --output "$forecast_out/controls"
sh test/run.sh outshine/src/generators/weather/OpenMeteo
sh test/run.sh outshine/src/base/format/Json
```

The controls compare unchanged captured bytes and 33 mutations/boundaries against
the independent Python normalizer, then query exact and unavailable UTC instants:
105 native process checks in total. They cover signed int64 limits, times beyond
double integer precision, missing/null fields, calm, cardinal winds, explicit
units, invalid values, count/size limits and malformed JSON/UTF-8. Native unit
tests additionally reject incorrect hashes and acquisition context. Process wall
times include startup, file IO and formatting; they are not decoder or frame times.

The observed invalid-UTF-8 counterexample is fixed in the shared JSON string
parser, including object keys. It checks continuation bytes, shortest encodings,
surrogates and the Unicode scalar limit against
[RFC 3629 §3–4](https://www.rfc-editor.org/rfc/rfc3629#section-3).
Existing JSON escape behavior is retained. Native regression cases cover the
valid scalar boundaries and malformed byte sequences.

These checks provide no live weather, rendered Place, GPU cost or A18 evidence.
The fixed forecast's known zero low-cloud cover still cannot drive the existing
low-deck recipe from total cloud cover alone.
