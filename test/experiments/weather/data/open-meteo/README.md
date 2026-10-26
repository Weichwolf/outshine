# Open-Meteo forecast reference

[#253](https://github.com/Weichwolf/outshine/issues/253) owns normalization and the
snapshot proposal. This directory contains one fixed offline input, separate from
the runtime source and asset caches. It requires Python 3.11 or newer; the
normalizer and controls use only the standard library.

Weather data by [Open-Meteo.com](https://open-meteo.com/), licensed under
[CC BY 4.0](https://creativecommons.org/licenses/by/4.0/), as stated in the
[provider licence](https://open-meteo.com/en/licence). The response bytes are
unmodified. Normalized output changes names and units and remains a separate file.
The receipt records the exact request, acquisition UTC, byte hash and transport.
Acquisition used the existing native `Fetching` / `Data::Transport` path. Running
this reference performs no networking or automatic source refresh.

```sh
weather_reference_out="${TMPDIR:-/tmp}/outshine-weather-normalization-$(git rev-parse --short HEAD)"
export PYTHONDONTWRITEBYTECODE=1
nice -n 10 python3 -W error test/experiments/weather/open_meteo_controls.py
nice -n 10 python3 -W error test/experiments/weather/open_meteo.py \
  --input test/experiments/weather/data/open-meteo/koerbersee-20261015.json \
  --receipt test/experiments/weather/data/open-meteo/koerbersee-20261015.receipt.json \
  --output "$weather_reference_out/normalized.json"
```

The [hourly API contract](https://open-meteo.com/en/docs) distinguishes instantaneous
fields from preceding-hour amounts. This bounded reference accepts one location,
consecutive hourly Unix timestamps and explicit units. It checks the response hash,
caps input size and preserves missing/null values as unknown; known zero stays zero.
Malformed values or incompatible units fail instead of being guessed or clamped.
Normalized schema v2 names the horizontal ENU vector `wind_horizontal_10m_east_north_m_s`;
wind-from north flows toward negative north. Render-frame conversion happens later.
This corrects the v1 reference's opposite second axis; original source bytes stay intact.

| Input | Normalized meaning |
| --- | --- |
| UTC Unix time | Signed 64-bit seconds; timezone offset remains metadata |
| Total/low/mid/high cloud cover | Separate fractions in [0, 1] |
| Wind speed/direction at 10 m AGL | m/s, wind-from degrees, derived ENU east/north velocity |
| Temperature/humidity at 2 m AGL | K, relative humidity fraction |
| Visibility/surface pressure | m, Pa |
| Precipitation/rain/showers | Previous-hour water-equivalent amount in kg/m² |
| Snowfall/snow depth | Previous-hour depth in m / instantaneous depth in m |

Requested WGS84 position and returned model-grid position remain separate. Returned
`elevation` is the surface elevation used for statistical downscaling, possibly the
request override; it is not independent evidence of grid-cell or camera height.
The automatic forecast mix and its model-run UTC remain unknown. Acquisition UTC
and `generationtime_ms` are not a model-run timestamp. Forecasts remain model data,
including returned hours before acquisition.

The capture starts with high-cloud fraction 0.30 and low-cloud fraction 0.00. Total
cloud cover therefore cannot directly select the existing low-deck recipe. Cloud
base, cloud-height wind and vertical wind remain unknown. Snow depth alone supplies
no snow mass or surface-history state. The scripts produce no rendered Place and
do not yet implement the public native WeatherSnapshot or a runtime provider.
