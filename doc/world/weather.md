# Weather — the `/wx` data kind and the FBWX format

**Sources of this file:** the former `doc/flightbox/world-and-terrain.md` §9 (German, translated and split off in
the Phase-3 mirror rebuild). Server side: `tiles/src/wx.c` (endpoint, run determination,
quantisation), `tiles/src/grib2.c` (GRIB2 decoder), `tiles/src/wxfmt.h` (**the format constants — that
header is the machine-readable part of this file**). Sim side: `sim/src/core/FBWeatherProvider.h`,
`FBCalmWeather.h`, `FBConstantWindWeather.h`, `FBFixedWeather.h/.cpp`, `FBWxFormat.h`. Fixture of a
real fetch: `tiles/testdata/wx-gfs-2026-07-27T00Z-step2-v1.wxb` (+ `manifest.txt`).

The **mission-author's** view of weather — the `wx` line, the three providers, the precedence rule and
the measured wind cases — is in [`../missions/weather.md`](../missions/weather.md). Its neighbour on
this side is [`terrain.md`](terrain.md), which documents the other three data kinds.

---

## Spec

The fourth data kind beside DEM, vector and imagery, and the only **untiled** one: worldwide wind and
cloud from the **NOAA GFS**, fetched on demand, cached, delivered in a compact binary format of its own
(`FBWX`).

### 9.1 Why ONE blob and not tiles

At 0.25° the global field of **one** variable is 1440×721 points — exactly the inverse of the DEM
situation: not a tiny cut-out from an enormous dataset but a dataset that is small as a whole. Three
reasons for a single package instead of one blob per variable:

1. The client wants **everything once per session**. N blobs = N round trips and N cache entries for a
   single logical thing.
2. **One run is one atmosphere.** Separate blobs could fall across a run boundary and give the client
   wind from the 06z and cloud cover from the 12z run. In the packed package the run timestamp is a
   **header field common to all 20 fields**, and the server refuses to write records of two runs into
   one package.
3. The header is self-describing (field list + offsets), so "everything in one" costs nothing in
   flexibility: whoever needs only the wind reads ten planes and ignores the rest.

### 9.2 The endpoint

```
GET /wx          → 200  application/octet-stream, FBWX blob (today 8,317,984 B)
                 → 503  no GFS run reachable AND none on disk
```

No query parameters, no path variants. The status-code contract is the existing one
([`terrain.md`](terrain.md) §7.1), applied to this data kind:

| Code | Meaning |
|---|---|
| **200** | terminal — bytes present. Including the case "oldest run from disk, NOMADS currently unreachable": the answer then carries `X-Wx-Stale: 1`. |
| **503** | no data, transient → retry. Is **never** cached (neither by nginx nor on disk). |
| **404** | only for a different path (`/wx/anything`) — there is exactly one resource. |
| **202** | does not occur. `/wx` **blocks** like `/bake` until the package is finished; the client's HTTP timeout is the only deadline. |
| **204** | does not occur. For DEM/vector, 204 distinguishes "a real hole" from "empty"; weather has **no hole** — the atmosphere covers every point on earth. A field without a value at a grid point (only the cloud ceiling) is marked **inside the blob**, not by a status code (§9.5). |

Response headers:

| Header | Content |
|---|---|
| `Cache-Control: public, max-age=N` | remaining lifetime of the delivered run: `run time + 6 h + 4 h − now`, at least 300 s. **No `immutable`** — as the only cacheable route. |
| `X-Wx-Format: FBWX/1` | format version, identical to the header field |
| `X-Wx-Run` / `X-Wx-Valid` | ISO-8601 UTC of the run resp. of the validity time (equal at f000) |
| `X-Wx-Stale: 0\|1` | 1 = the run could not be confirmed against NOMADS in this pass (network gone) |
| `X-Cache-Status` | from nginx, as on all cacheable routes |

nginx's own `location = /wx` block is the only cacheable route without `immutable`: the origin computes
the remaining lifetime of the delivered GFS run and states it as `Cache-Control: max-age`. nginx
evaluates the response headers BEFORE `proxy_cache_valid`, so the origin decides; `proxy_cache_valid
200 1h` is only the fallback. 503 gets **no** validity rule → a NOMADS outage can never be stored as
weather.

### 9.3 Source and run determination

NOAA GFS via NOMADS, **0.25°**, without a key. The fetch goes through the `grib_filter` CGI
(`https://nomads.ncep.noaa.gov/cgi-bin/filter_gfs_0p25.pl`), which cuts server-side by variable and
level — the full GRIB (~500 MB) is never loaded. Time step **f000** (analysis) = the simulator's "now";
`run == valid`.

**Three** filter requests, not one: the CGI delivers the **cross product** of the chosen variables and
levels, so a single collective request would additionally pull TCDC on the four pressure surfaces and
HGT at the surface (~5 MB of waste). The three groups hit exactly the 20 wanted records.

**Run determination** (`wx_acquire`, `wx.c`): GFS runs 4×/day (00/06/12/18Z) and lands on NOMADS ~3.5–5
h after its analysis time. The server tries from the **newest cycle backwards**, per cycle first disk,
then network:

* **Not ready** — NOMADS answers with 404 *or* with `200` and an HTML error page ("Data file is not
  present"). The status code alone therefore proves nothing; the actual distinction is the **GRIB magic
  at the start of the body** plus the completeness of all 20 records. Precisely that check is the reason
  why an error page can never land here as weather in the cache (the `/elev` lesson). Reaction: the next
  older cycle (up to 4 = 24 h back).
* **Not reachable** (connection error or 5xx) — the network path is given up **immediately** instead of
  knocking on four more cycles; the rest of the pass searches only on disk (up to 8 cycles = 48 h) and
  delivers the newest thing there is, with `X-Wx-Stale: 1`.
* **Nothing at all** — 503, and nothing is written.

After every pass that found nothing new, a **damper** (`g_next_probe`, 600 s) blocks further NOMADS
probes; otherwise every request in the gap between two runs would knock again. Simultaneous requests
share **one** build (a mutex plus condvar, one artefact = one in-flight key): 16 parallel cold starts →
`wx_built=1`.

### 9.4 The grid

| | |
|---|---|
| Source grid | GFS 0.25°, 1440×721, row 0 = 90 °N, column 0 = 0 °E (scan mode 0) |
| Output grid | every **`grid_step`**-th source point on an exact subgrid; default `grid_step = 2` → **0.5°, 720×361** |
| Why subsampled and not averaged | this way **every delivered value is literally a GFS grid-point value** and not a number invented by us — which is also what makes the point comparison against an independent GFS consumer (§9.7) meaningful in the first place. For synoptic fields the loss is meaningless: the 0.25° product is itself already the output of a more coarsely resolving spectral model. |
| Why `grid_step` at all | at 0.25° the same field set would be **33 MB** — far beyond what a client pulls once per session. `grid_step 2` ⇒ 8.3 MB, `4` ⇒ 2.1 MB. It is a **compile-time macro** (`FB_WX_STEP` in `wx.c`), not a query parameter: no second cache identity, no per-request branch. |

The client encodes **nothing** of this: `nx`, `ny`, `lat0`, `lon0`, `dlat`, `dlon` stand in the header.

```
sample(i,j) lies at      lat = lat0 + j*dlat      (90 − 0.5·j)
                         lon = lon0 + i*dlon      (0.5·i)
index in the plane:      idx = j*nx + i
```

`dlat` is **negative** (row 0 = the north pole). Longitudes run 0…359.5 °E; `flags` bit 0
(`FB_WX_HDR_FLAG_LON_WRAP`) says that column `nx−1` lies exactly one `dlon` before column 0 —
bilinearly, take `i` modulo `nx`. Latitudes do **not** wrap: clamp `j` to `[0, ny−1]`.

### 9.5 The `FBWX` format

Everything **little-endian**. Layout: header (64 B) + `field_count` descriptors (24 B each) + the
planes. Every descriptor carries its **absolute** offset, so a reader never has to sum up sizes.

**Header, 64 bytes:**

| Off | Type | Field | Meaning |
|---:|---|---|---|
| 0 | u32 | `magic` | `0x58574246` = `'F','B','W','X'` |
| 4 | u16 | `format_version` | today **1**. Changes as soon as a byte means something else. |
| 6 | u16 | `header_bytes` | 64 — where the descriptors begin (grows in future versions) |
| 8 | u16 | `nx` | columns (720) |
| 10 | u16 | `ny` | rows (361) |
| 12 | f32 | `lat0` | latitude of row 0 (+90.0) |
| 16 | f32 | `lon0` | longitude of column 0 (0.0) |
| 20 | f32 | `dlat` | latitude step per row (**−0.5**) |
| 24 | f32 | `dlon` | longitude step per column (+0.5) |
| 28 | u32 | `run_epoch` | analysis time of the GFS run, UTC seconds — **from the GRIB itself**, not from the URL |
| 32 | u32 | `valid_epoch` | validity time (= `run_epoch` at f000) |
| 36 | u32 | reserved | 0 |
| 40 | u16 | `field_count` | 20 |
| 42 | u16 | `desc_bytes` | 24 — stride of the descriptor table |
| 44 | u32 | `payload_bytes` | sum of all planes (8,317,440) |
| 48 | u8 | `flags` | bit 0 = longitude wraps |
| 49 | u8 | `source` | 1 = NOAA GFS 0.25° |
| 50 | u16 | `grid_step` | source points per output point (2) |
| 52 | u8[12] | reserved | 0 |

**Descriptor, 24 bytes:**

| Off | Type | Field | Meaning |
|---:|---|---|---|
| 0 | u8 | `var` | 1 `WIND_U` (m/s, eastward) · 2 `WIND_V` (m/s, northward) · 3 `HEIGHT` (m) · 4 `CLOUD` (%) · 5 `VIS` (m) |
| 1 | u8 | `level_kind` | 1 `AGL` · 2 `ISOBARIC` · 3 `CLOUD_LOW` · 4 `CLOUD_MID` · 5 `CLOUD_HIGH` · 6 `ATMOSPHERE` · 7 `SURFACE` · 8 `CLOUD_CEIL` |
| 2 | u16 | `level_value` | metres for `AGL`, hPa for `ISOBARIC`, otherwise 0 |
| 4 | u8 | `bits` | 8 or 16 |
| 5 | u8 | `flags` | bit 0 = the field knows "no value" |
| 6 | u16 | `missing_raw` | this raw value means "no value" (only if bit 0 is set) |
| 8 | f32 | `scale` | |
| 12 | f32 | `offset` | |
| 16 | u32 | `payload_off` | absolute byte offset of the plane in the blob |
| 20 | u32 | `payload_bytes` | `nx·ny·bits/8` |

**Value reconstruction** — one line, the same for every field:

```
raw   = 8-bit byte or 16-bit LE word at  payload_off + (j*nx + i) * bits/8
value = offset + raw * scale             (unit from `var`)
```

and, if `flags & 1` and `raw == missing_raw`: **no value** (not 0, do not interpolate — exclude
neighbouring points that are likewise "no value" from the interpolation).

The unit follows from `var`, which is why there is no unit field. `HEIGHT` is **geopotential height
above MSL** (GFS parameter "Geopotential height", gpm) — the difference from geometric metres is below
0.3 % at 11 km, irrelevant for assigning a wind level to a flight altitude.

### 9.6 The 20 fields and their quantisation

The quantisation windows are **hard-wired, never derived from the data**: the same input gives the same
bytes, and a client may hard-code the meaning of a raw value. Values outside the window **saturate**
(clamp), they never wrap.

| # | var | level | bits | `scale` | `offset` | `missing_raw` | `payload_off` | Bytes | Resolution |
|---:|---|---|---:|---|---:|---|---:|---:|---|
| 0 | `WIND_U` | `AGL` 10 m | 16 | 0.00549324788 | −180 | — | 544 | 519,840 | 0.0055 m/s |
| 1 | `WIND_V` | `AGL` 10 m | 16 | 0.00549324788 | −180 | — | 520,384 | 519,840 | 0.0055 m/s |
| 2 | `WIND_U` | `ISOBARIC` 850 | 16 | 0.00549324788 | −180 | — | 1,040,224 | 519,840 | 0.0055 m/s |
| 3 | `WIND_V` | `ISOBARIC` 850 | 16 | 0.00549324788 | −180 | — | 1,560,064 | 519,840 | 0.0055 m/s |
| 4 | `WIND_U` | `ISOBARIC` 700 | 16 | 0.00549324788 | −180 | — | 2,079,904 | 519,840 | 0.0055 m/s |
| 5 | `WIND_V` | `ISOBARIC` 700 | 16 | 0.00549324788 | −180 | — | 2,599,744 | 519,840 | 0.0055 m/s |
| 6 | `WIND_U` | `ISOBARIC` 500 | 16 | 0.00549324788 | −180 | — | 3,119,584 | 519,840 | 0.0055 m/s |
| 7 | `WIND_V` | `ISOBARIC` 500 | 16 | 0.00549324788 | −180 | — | 3,639,424 | 519,840 | 0.0055 m/s |
| 8 | `WIND_U` | `ISOBARIC` 250 | 16 | 0.00549324788 | −180 | — | 4,159,264 | 519,840 | 0.0055 m/s |
| 9 | `WIND_V` | `ISOBARIC` 250 | 16 | 0.00549324788 | −180 | — | 4,679,104 | 519,840 | 0.0055 m/s |
| 10 | `HEIGHT` | `ISOBARIC` 850 | 8 | 4.70588255 | 600 | — | 5,198,944 | 259,920 | 4.7 m |
| 11 | `HEIGHT` | `ISOBARIC` 700 | 8 | 5.88235283 | 2000 | — | 5,458,864 | 259,920 | 5.9 m |
| 12 | `HEIGHT` | `ISOBARIC` 500 | 8 | 7.05882359 | 4300 | — | 5,718,784 | 259,920 | 7.1 m |
| 13 | `HEIGHT` | `ISOBARIC` 250 | 8 | 11.7647057 | 8500 | — | 5,978,704 | 259,920 | 11.8 m |
| 14 | `HEIGHT` | `CLOUD_CEIL` | 16 | 0.305185109 | 0 | **65535** | 6,238,624 | 519,840 | 0.31 m |
| 15 | `CLOUD` | `ATMOSPHERE` | 8 | 0.392156869 | 0 | — | 6,758,464 | 259,920 | 0.39 % |
| 16 | `CLOUD` | `CLOUD_LOW` | 8 | 0.392156869 | 0 | — | 7,018,384 | 259,920 | 0.39 % |
| 17 | `CLOUD` | `CLOUD_MID` | 8 | 0.392156869 | 0 | — | 7,278,304 | 259,920 | 0.39 % |
| 18 | `CLOUD` | `CLOUD_HIGH` | 8 | 0.392156869 | 0 | — | 7,538,224 | 259,920 | 0.39 % |
| 19 | `VIS` | `SURFACE` | 16 | 0.373846024 | 0 | — | 7,798,144 | 519,840 | 0.37 m |

Why these widths:

* **Wind 16 bit over ±180 m/s.** The strongest jet stream ever measured is around 110 m/s; the window is
  deliberately wide and the resolution of 0.0055 m/s is still an order of magnitude finer than the model
  uncertainty. The source itself quantises more coarsely (GFS packs UGRD to 9–13 bits).
* **Heights 8 bit with a narrow window per pressure surface.** A geopotential surface varies globally
  only by 800–2100 m; the windows above leave ≥400 m of reserve on each side and still resolve 5–12 m.
  They serve to **locate** a wind level in metres, not to set altimeters.
* **Cloud cover 8 bit.** The value range is 0–100 %, and GFS itself delivers only one decimal.
* **Visibility 16 bit, although it is a "cloud field".** Visibility matters precisely where it is small:
  at 8 bit, 200 m of fog would be uncertain to ±47 m. GFS caps "unlimited" at ~24,135 m; the window goes
  to 24,500 m.
* **Cloud ceiling with a real missing flag.** GFS reports "no ceiling" not by a bitmap but as a height of
  ~20,000 m. Values ≥ 19,000 m become `missing_raw = 65535` here — a renderer must not draw a cloud base
  at 20 km. Share in the example: 47.2 % of the grid points, exactly congruent with `GFS ≥ 19000`
  (verified).

**Cloud ceiling versus pressure:** checked — the GFS `f000` analysis step delivers the ceiling as
`HGT:cloud ceiling` (geopotential height in m). A `PRES:cloud base` record does **not** exist in this
step; the height is therefore not merely the more convenient but the only form.

**Cloud cover, which records:** in the analysis step the layers are called `LCDC`/`MCDC`/`HCDC`
(low/middle/high cloud layer, instantaneous) and the total cover `TCDC:entire atmosphere` — not, as in
the forecast steps, four times `TCDC` with different levels.

### 9.7 GRIB2 — why the decoder is our own code

The pragmatic route would have been `wgrib2` or the eccodes tools in the container. Measured:

* **`wgrib2` is not packaged in Debian trixie** (`apt-cache policy wgrib2` → empty).
* `libeccodes-tools` exists but pulls in `libeccodes0` + `libnetcdf22` (~40 MB in the image) and would
  have meant a `fork/exec` per fetch plus either text parsing of 20 million values or an intermediate
  file.

Instead: **`tiles/src/grib2.c`**, ~330 lines. The reason that is defensible stands in the data — all 20
GFS records use the same, narrowest subset of the standard:

| | |
|---|---|
| Grid | template **3.0** (regular lat/lon), scan mode 0 |
| Product | template **4.0** (4.8 also accepted) |
| Packing | template **5.3** — complex packing + second-order spatial differencing, `mvm = 0`, no bitmap. (5.0 simple packing is implemented as well.) |
| **Not** included | JPEG2000 (5.40), PNG (5.41), bitmaps in section 6, missing-value management in 5.3 |

Whatever does not belong is **rejected by name** (`fb_grib2_last_error`) instead of guessed — it is an
upstream answer, therefore a system boundary. One subtlety that stands clearly in no WMO table and that
a re-implementer must know: in section 7 the three descriptor arrays (group reference values, widths,
lengths) each begin **at an octet boundary**; without that padding the sum of the group lengths does not
give the point count (first debug finding: 1,468,303 instead of 1,038,240).

**Verification against independent references.** Two, because they prove different things:

*1 — ecCodes 2.41 (container, `grib_get -l lat,lon,1`), the authoritative decoding check* at five
points, GFS 2026-07-27 00Z:

| Point | Quantity | ecCodes | `/wx` | Δ | Quantisation step |
|---|---|---|---|---|---|
| 46.5 N / 7.5 E | u 250 hPa | 5.64641 m/s | 5.6443 | 0.0021 | 0.0055 |
| | v 250 hPa | −23.3125 | −23.3106 | 0.0019 | 0.0055 |
| | gh 250 hPa | 10,804.5 m | 10,805.9 | 1.4 | 11.8 |
| | vis | 24,134.8 m | 24,134.8 | 0.0 | 0.37 |
| 35.0 N / 139.5 E | ceiling | 5,948.38 m | 5,948.4 | 0.02 | 0.31 |
| | tcc / lcc / hcc | 95.5 / 29.7 / 95.5 % | 95.7 / 29.8 / 95.7 | ≤0.2 | 0.39 |
| 60.0 S / 60.0 W | 10u | 15.1738 m/s | 15.1751 | 0.0013 | 0.0055 |
| | lcc / mcc / hcc | 87.8 / 14.5 / 0 % | 87.8 / 14.5 / 0.0 | 0.0 | 0.39 |

Over **all 20 fields × all 259,920 grid points** the maximum error against an independent full-field
decoding is exactly **0.5 quantisation steps** — i.e. pure rounding, not a decoding error; the missing
mask of the cloud ceiling agrees point by point.

*2 — Open-Meteo (`api.open-meteo.com/v1/gfs`, `models=gfs_global`), an independent operational GFS
consumer*, 46.5 N / 7.5 E, 2026-07-27T00:00Z:

| Quantity | Open-Meteo | `/wx` |
|---|---|---|
| Wind 250 hPa | 24.01 m/s from 346° | 23.98 m/s from 346.4° |
| gh 250 hPa | 10,803.81 m | 10,805.9 m |
| Wind 850 hPa | 2.04 m/s from 210° | 2.11 m/s from 210.5° |
| gh 850 hPa | 1,520.0 m | 1,522.4 m |
| Wind 10 m | 2.08 m/s from 215° | 2.34 m/s from 208.4° |
| Visibility | 24,140 m | 24,134.8 m |

Wind direction/magnitude and heights agree to <0.1 m/s resp. <2.5 m — the height difference is exactly
a quarter of a quantisation step. The 10 m wind deviates somewhat more because Open-Meteo interpolates
spatially and downscales terrain-dependently (it reports its own terrain height for the point), while
`/wx` delivers the raw grid point. **Cloud cover is not suitable as a comparison**: Open-Meteo's GFS
cloud layers are computed from relative humidity, not the GFS's own LCDC/MCDC/HCDC diagnostics — at
60 S/60 W Open-Meteo reports "high 100 %" while GFS itself (confirmed via ecCodes) says 0 %. For the
clouds, ecCodes is the reference, not Open-Meteo.

### 9.8 Determinism and the gym fixture

A blob is a **pure function of (GFS run, format version, `grid_step`)**. There is no creation timestamp
in it (offset 36 is deliberately reserved and zero), no random number, no data-derived quantisation. Two
independent cold starts of the same cycle — different machines, different compilers (clang/macOS versus
gcc/Debian in the container) — deliver **byte-identical** 8,317,984 bytes (measured, md5
`17b33c82bafae29442eb6d1cc12fb6de`).

For the baked-in fixture that means: `tiles/testdata/wx-gfs-2026-07-27T00Z-step2-v1.wxb`, sha256
`acded0200d49926203d4548301a2fd1586b6e3c5ecbf61fbd0355e6f9c609ede`. It is an unaltered 200 body of
`/wx` and can be adopted as a fixed gym weather situation; a regression test may compare it by checksum
instead of by tolerance. The file names on disk carry `grid_step` and the format version
(`gfs2_2026072700_v1.wxb`), so like the bake file names they are coupled to the version — a format
change moves the artefact instead of poisoning the old one.

### 9.9 Operating figures

Measured on 2026-07-27, GFS cycle 00Z, Apple A18 Pro, Podman VM:

| Quantity | Value |
|---|---|
| Raw bytes from NOMADS | **15,451,174 B** GRIB2 in 3 filter requests (7,313,561 + 4,463,283 + 3,674,330) |
| Delivered bytes | **8,317,984 B** (factor 0.54 against raw, at 20 M → 260 k grid points per field) |
| Over the wire (nginx gzip) | **4,599,798 B** |
| GRIB2 decoding, 20 fields / 20.8 M points | **0.10 s** (3 runs: 0.103 / 0.102 / 0.099) |
| Cold, origin direct | **7.30 s** — practically entirely NOMADS latency (of which 0.10 s decoding) |
| Cold, through nginx (`X-Cache-Status: MISS`) | **7.06 / 7.49 s** (two runs on an empty cache) |
| From disk after a restart (incl. one NOMADS probe) | **0.26 s** |
| Resident in the origin | **0.002–0.005 s** |
| Repeat through nginx (`X-Cache-Status: HIT`) | **0.081 s** |
| 16 simultaneous cold starts, origin direct in the container | 6.93 s wall, **`wx_built = 1`**, all 16 answers byte-identical (host binary: 6.12 s, same result) |
| 24 simultaneous requests through nginx after a MISS | 0.53 s wall, **24× `HIT`**, origin untouched |
| Memory during the build | blob 8.3 MB + one GRIB group ≤7.3 MB + decoder scratch 8.3 MB |
| Disk in the origin | one cycle = 8.3 MB; older ones are cleared away **by name** after the build (no `readdir`), steady state ≤ 8 files ≈ 75 MB |

Size at other `grid_step`: 1 → 33.2 MB · **2 → 8.3 MB** · 4 → 2.1 MB.

`/health` gets a group of its own:

```
wx_served=N wx_built=N wx_disk_hits=N wx_fetch_fail=N wx_decode_fail=N wx_stale_served=N wx_run_fallback=N
```

`wx_run_fallback` counts the passes in which the newest cycle was not yet published and the previous one
was fallen back to — the normal case in the ~4 h after an analysis time, not an error. `wx_fetch_fail`
and `wx_decode_fail` are the real error counters.

## State

### 9.10 What the simulator builds from it (built, commit 43b82b5)

The consumer exists. `core/FBWeatherProvider` is the sibling of `FBElevationProvider` —
`WindNedMs(lat,lon,altM)` (linear between the pressure surfaces over their own geopotential; below the
10 m level the 10 m field, above 250 hPa the topmost surface holds), `CloudLayers(lat,lon)`,
`VisibilityM(lat,lon)`. Four implementations: `FBCalmWeather` (default, no wind),
`FBConstantWindWeather` (one vector everywhere — a measuring instrument, not weather), `FBFixedWeather`
(the FBWX blob, from a FILE for the gym and from MEMORY for the browser) and, as pure configuration
rather than a fifth class, the live path: the browser fetches `/wx` once per session and constructs the
same `FBFixedWeather`. The MIRROR of the format lies in `sim/src/core/FBWxFormat.h` (`core/` must not
point at `tiles/`); against drift stands `build/fb-test-weather`, which parses the fixture and
recomputes §9.7's ecCodes sample values with the quantisation step of each field as the tolerance.

| Item | State |
|---|---|
| `/wx` endpoint | built; one blob, three filter requests, run determination with disk fallback and stale flag |
| GRIB2 decoder | built; ~330 lines, templates 3.0/4.0/5.0/5.3, everything else rejected by name |
| FBWX format | built and frozen at version 1; 64 B header, 24 B descriptors, 20 fields |
| Determinism | measured: byte-identical across machines and compilers, md5 `17b33c82…` |
| Sim-side consumer | built (`43b82b5`); four provider implementations, mirror header, drift checker |
| Mission declaration | built — see [`../missions/weather.md`](../missions/weather.md) |

## Gaps

| Gap | Detail |
|---|---|
| **Only the analysis step f000, no forecast (`fXXX`)** | server-side open and deliberately left so: a session longer than the run sees the same atmosphere until nginx pulls the next run. The structure carries it (`valid_epoch` is a header field of its own, `parse_product` already computes forecast times), there is simply no consumer yet that would need a time axis. |
| The 10 m surface is anchored at 10 m ASL, not above ground | the one approximation, documented in the provider; a terrain-dependent boundary-layer wind would need the elevation hook as a second input |
| Cloud rendering does not consume the provider yet | it is reached through `FBWorld::SetWeather/Weather()` (borrowed, like the unit registry); the rendering side is the cloud round — see [`../render/clouds.md`](../render/clouds.md) |
| Terrain masking is unrelated but still open | named here only because §9.10 listed it together with the time axis; it belongs to [`terrain.md`](terrain.md) |
| The format has no per-field unit code | the unit follows from `var`, which is compact but means a new variable kind needs a format-version bump |
| Only one missing-value field | only the cloud ceiling declares "no value"; the other 19 fields saturate at their window edges instead |

## Knowledge

- **Why one package and not one blob per variable.** The client wants everything once per session; N
  blobs would be N round trips and N cache entries. More importantly, one run is one atmosphere —
  separate blobs could straddle a run boundary and hand out 06z wind with 12z cloud. In the package the
  run timestamp is a header field common to all 20 fields, and the server refuses to mix runs.
- **Why 204 does not exist on `/wx`.** For DEM and vector, 204 distinguishes "a real hole" from "empty".
  Weather has no hole: the atmosphere covers every point on earth. The one field that can lack a value
  at a grid point (the cloud ceiling) marks that inside the blob, not by a status code.
- **Why the GRIB magic and not the status code decides.** NOMADS answers a not-yet-published run either
  with 404 or with 200 plus an HTML error page. Checking the body's magic plus the completeness of all
  20 records is what keeps an error page from ever being cached as weather — the same lesson as the
  strict `/elev` parse.
- **Why subsampling and not averaging.** Every delivered value is then literally a GFS grid-point value,
  which is what makes a point comparison against an independent GFS consumer meaningful at all. An
  averaged value would be a number of ours, and no external reference could confirm it.
- **Why `grid_step` is a compile-time macro.** As a query parameter it would create a second cache
  identity and a per-request branch, for a knob nobody turns per request.
- **Why the quantisation windows are hard-wired.** Derived from the data they would make the same input
  produce different bytes, which would destroy the determinism the fixture and its checksum rest on.
- **Why the decoder is our own ~330 lines.** `wgrib2` is not packaged in Debian trixie, and
  `libeccodes-tools` would have cost ~40 MB in the image plus a fork/exec per fetch. All 20 records use
  the same narrow subset of the standard; anything outside it is rejected by name rather than guessed.
- **The octet-boundary subtlety in section 7.** The three descriptor arrays each begin at an octet
  boundary. Without that padding the sum of the group lengths does not match the point count — the first
  debug finding was 1,468,303 instead of 1,038,240.
