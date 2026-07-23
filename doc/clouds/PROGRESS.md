# Cloud/Haze Research — PROGRESS

Task: distill real-time volumetric cloud + haze/atmosphere state of the art into implementation-ready
doc under `doc/clouds/`, cross-referenced against FlightBox's current cloud pass
(`command_center/fb/FBRenderer.cpp`). See [INDEX.md](INDEX.md) for the source list.

## Source coverage

| Source | Extraction method | Coverage |
|---|---|---|
| Schneider & Vos, SIGGRAPH 2015 ("HZD Cloudscapes") | Downloaded PDF (`guerrilla-games.com/read/...`), `pdftotext -layout`, all 99 pages/96 numbered slides read in full | **Complete** — noise (§A), density/height (§B), lighting (§C), march (§D), temporal (§E) all sourced from this deck |
| Schneider, Nubis³ 2023 (recaps "Nubis, Evolved" 2022) | Downloaded PDF (220 pages), `pdftotext -layout`; read in full through the cloud-relevant sections (slides 1–60, the vertical-profile/envelope/voxel history); the later voxel-cloud deep-dive (slides ~60–220) skimmed for headline numbers only (not fully distilled — see below) | **Sections 1–60 complete**; voxel-cloud internals (slides 60–220, "Nubis3 / Voxel Clouds / Sampling Density" onward) **not fully distilled** — not needed for FlightBox's noise-composite approach, flagged as future work if a voxel-based redesign is ever considered |
| Hillaire, SIGGRAPH 2016 Frostbite course | Downloaded PPTX (231 MB, embedded video), extracted slide XML text directly (`unzip` + regex on `<a:t>` runs) to avoid the media payload; all 64 slides read | **Complete** — exact HG g-values, energy-conserving integration, cloud/AP coupling, XB1 perf numbers, Sunny-16 anchor all sourced from this deck |
| Hillaire, EGSR 2020 (atmosphere LUTs) | Not re-fetched — already implemented verbatim in `FBRenderer.cpp`'s `kAtmoCommon`/`kTransmittanceCS`/`kSkyViewCS`; referenced by pointing at that existing code, not re-derived | **Complete by reference** |
| Wrenninge, "Oz: The Great and Volumetric" SIGGRAPH 2010 Talks | Not directly accessible (talk recording/slides not found freely hosted); cited only via Hillaire 2016's reference to it (slide 61 `[Wrenninge10]`) | **Partial** — architecture confirmed, exact per-octave multiplier constants NOT found in any freely accessible primary source; flagged as estimated throughout |
| Koschmieder visibility formula | Confirmed via search across multiple secondary atmospheric-optics sources (consistent `σ=3.912/VIS`, all agreeing) | **Complete** — single formula, no ambiguity found |
| Maxime Heckel blog (secondary) | WebFetch | **Complete** — used only as a pedagogical cross-check, not a primary citation |
| Nijhoff "Himalayas" blog (secondary) | WebFetch | **Complete** — used only to confirm the 128³/32³ texture split independently |

## Files written

| File | Section | Status |
|---|---|---|
| `INDEX.md` | — | Done |
| `01-noise-construction.md` | A | Done |
| `02-density-coverage.md` | B | Done |
| `03-lighting-model.md` | C | Done |
| `04-raymarch-strategy.md` | D | Done |
| `05-temporal-reprojection.md` | E | Done |
| `06-haze-aerial-perspective.md` | F | Done |
| `07-igpu-performance-budget.md` | G | Done |
| `08-experiment-protocol.md` | H | Done |
| `09-current-state-gaps.md` | 5 findings | Done |
| `PROGRESS.md` | — | This file |

## Remaining / explicitly out of scope for this run

- Nubis³'s voxel-cloud internals (slides ~60–220 of the 220-page deck) — skimmed for headline numbers,
  not fully distilled; only relevant if FlightBox ever moves to a full 3D voxel density field instead
  of the noise-composite approach, which [07-igpu-performance-budget.md §5](07-igpu-performance-budget.md)
  argues against on this hardware class anyway.
- No implementation/code changes were made — this is documentation only, per task scope. The fixes
  named in [09-current-state-gaps.md](09-current-state-gaps.md) are recommendations, not applied
  patches.
- Wrenninge's exact multi-scatter octave constants remain unverified against a primary source (only
  cited secondhand via Hillaire); if a future run finds the actual 2010/2013 talk material, update
  [03-lighting-model.md §4](03-lighting-model.md).

## COVERAGE: COMPLETE

All 8 requested content sections (A–H) plus the mandatory 5-findings closing deliverable are written,
cited, and cross-referenced against the current codebase. The one explicitly incomplete sub-area
(Nubis³ voxel-cloud internals) is out of scope for FlightBox's current noise-composite architecture and
is flagged rather than silently omitted.
