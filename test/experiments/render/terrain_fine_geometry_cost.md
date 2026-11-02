# Fine terrain geometry diagnostics

Owner: [#1934](https://github.com/Weichwolf/outshine/issues/1934).
Basis: `f9d8af17a`, A18 Pro/Metal, 1280x720/60, offline, no vegetation.

## Method

An isolated executable/shader package avoids touching production artifacts. Its
asset directory links the existing DB; no DB copy. A separately compiled original
`groundPatchLit-100.frag.spv` is byte-identical to the shipped artifact. Two private
overrides have identical reflected resources and interfaces:

- Coarse normal returns the existing interpolated vertex normal instead of
  reconstructing fine normals for coarse patches.
- Coarse receiver uses interpolated light-space position and its screen-space
  receiver-plane derivative instead of reconstructing the fine triangle.

These substitutions change appearance. Normal changes also affect slope-based
material selection; receiver changes affect shadow sampling. Differences are
coupled diagnostic frame deltas, not exclusive function costs, additive savings
or a justified replacement. No production simplification is admitted.

Each scene runs original/normal/receiver/receiver/normal/original, each in a fresh
process with 240 measured frames/four rotations. Final-60 GPU medians below retain
one full turn after initial use; they do not replace the regular 60-frame Place
gate. No concurrent builds; timing runs have Metal API validation unset. Worlds
are complete with zero unrefined frames; only the final image is stored per run.

| Scene | Original medians ms | Coarse normal ms | Coarse receiver ms |
|---|---|---|---|
| Jura | 21.621 / 21.733 | 21.122 / 20.934 | 20.930 / 21.075 |
| Koerbersee | 21.478 / 21.569 | 21.545 / 21.523 | 21.465 / 21.573 |

Jura's paired median differences are about 0.65 ms, while Koerbersee has no useful
median gain. Koerbersee's all-240 GPU-p99 falls from 37.14–37.19 ms to
36.33–36.76 ms (normal) or 36.13–36.44 ms (receiver); all remain over budget.
The finite observations do not establish a general cost bound. Fine normal and
receiver reconstruction remain enabled; no new normal cache or texture format.

## Source review and next decision

For a coarse patch, the source requests three four-neighbour node normals and
three triangle heights. Fifteen logical fetch calls do not imply fifteen physical
transactions; compiler reuse and texture caches remain separate. Opaque geometry
already precedes the ground depth pyramid and ground selection. Repeating the
rejected full terrain depth prepass from #1919 is not justified.

The R8 mask experiment still has no whole-frame gain. Its World/feedback adapter
and 84 shader permutations add 16,069,176 artifact bytes without a default image
benefit. The experimental frame integration is withdrawn; independent native
sampling/cache references remain. Full static material reuse is an open requirement.

Raw commands, original/override sources, reflected layouts, shader/binary hashes,
all GPU samples and images: `/tmp/outshine-master-1934-fine-cost/` on the owner host.
Preparation/run drivers share that prefix with `-prepare.py` / `-run.py`.
The private package restores its original shader after every run series.
