# Static playback ownership

Owner: [#15](https://github.com/Weichwolf/outshine/issues/15).

`ScenePlayback::SetGeometry` takes ownership of a generated world. Keeping a
second full snapshot immediately is unnecessary: static sampling does not pose
or rebuild it. The snapshot is now materialized only when another native or
imported scene actually joins it. Failed composition preserves the original
owner and retains no deferred snapshot. Imported animation still retains its
individual source snapshots and rebuilds its poses.

The regression test transfers a 3 MiB position stream without increasing live
heap bytes, checks a rejected addition, and composes native owners with an
animated glTF fixture. All 46 checks pass. Against the unchanged implementation,
the correct baseline test fails only the allocation check. Five affected
GroundWorldCandidate integration tests pass; changed production/test units pass
focused clang-tidy and formatting.

A18 Pro, macOS, 1280×720/30: before / after / after / before, each with Wien and
Koerbersee. Fresh complete offline processes at normal priority, no concurrent
owned heavy job, exactly 30 frames and 30 valid GPU timestamps per process.
`/usr/bin/time -l` supplies peak footprint and RSS separately.

| Peak footprint, bytes | Before, two runs | After, two runs |
|---|---:|---:|
| Wien | 3,075,426,944 / 3,077,933,696 | 2,894,793,200 / 2,938,440,224 |
| Koerbersee | 1,670,301,400 / 1,667,516,120 | 1,657,112,352 / 1,653,343,936 |

For Wien, the difference of the two means is
`(3,075,426,944 + 3,077,933,696 - 2,894,793,200 - 2,938,440,224) / 2`
`= 160,063,608 bytes = 152.65 MiB`, or 5.20% of the old mean.
Koerbersee saves 13,680,616 bytes, or 13.05 MiB / 0.82%.
These are measured process-footprint differences, not a complete allocation
inventory. RSS varies with residency/compression and establishes no gain.
GPU maxima vary; this change establishes no GPU speedup.

All eight paired PNGs preserve their respective hashes. A subsequent complete
twelve-Place capture also preserves every prior PNG byte for byte. Wien,
Koerbersee and Tokyo were opened again; the other identical images retain their
preceding visual review. Fragmented near vegetation, crude shore contacts and
flat materials remain visible deficiencies.

In that twelve-Place capture, GPU maxima exceed `1000/30 = 33.333 ms` in Tokyo
(33.617 ms), Central Park (37.782 ms) and Koerbersee (36.500 ms). Wien exceeds
the one-second turn limit at 1001.958 ms. Eight Places hold all measured limits;
all CPU-p99 are below 7.1 ms. These changing spikes require further work and do
not admit the native port to master. Source coverage, profile, view distance,
shader math and rendered contents are unchanged.
