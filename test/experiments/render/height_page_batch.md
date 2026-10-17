# Production height-page upload comparison

Issue: [#441](https://github.com/Weichwolf/outshine/issues/441).

`height_page_batch.cpp` exercises the actual `SceneRenderer` height-page arena,
CPU selection metadata, atlas growth and native upload queue. It accepts a complete
sequence of little-endian 35×35 float32 pages, including the existing one-node halo.
The old checkout uses scalar calls; the candidate uses groups of at most 48 pages.
Each executable uses its checkout's complete SDK and matching internal headers.
The experiment adds no public GPU or SDK interface.

Build each checkout normally and run `make db` with its own compiler/dependencies.
The driver supports Python 3.9 or newer and uses the repository's native linker
flags and process supervisor. It requires neither GNU timeout nor coreutils.
Then invoke the driver from the candidate, with all private outputs under `/tmp`:

```sh
python3 test/experiments/render/height_page_batch.py \
  --checkout /tmp/outshine-before-COMMIT --scalar \
  --input /tmp/outshine-inputs/height-pages.f32 \
  --output /tmp/outshine-height-before/result.json
python3 test/experiments/render/height_page_batch.py \
  --checkout /tmp/outshine-after-COMMIT \
  --input /tmp/outshine-inputs/height-pages.f32 \
  --output /tmp/outshine-height-after/result.json
```

The timed interval starts after loading the input, opening the renderer and
settling setup uploads. It includes CPU copies, metadata construction, native
recording/submission, backpressure retries and completion of the final upload.
It is host elapsed time, not GPU execution time. Successful staging allocations,
their requested bytes, accepted submissions and atlas-growth copy bytes are
counted only in this interval. Backpressure may cause additional attempted work.

After timing, the probe reads every occupied physical GPU page and compares every
float bit with the original input. Readback, verification and renderer setup are
excluded from upload time. Whole-process peak RSS includes them and the input;
it is neither upload-only memory nor device memory. The bounded Python supervisor
is included in whole-process resource accounting. Build and run logs, input,
probe and SDK hashes, source commit and dirty state accompany the result. Run
comparisons serially with the same host load and keep both original inputs.

The full measured inputs are Flensburg: 4,132 pages / 20,246,800 bytes, SHA-256
`59d62d482f601471db16d3d183a0e448e1fec320af5260cd55c7fe47ce7080d0`,
and Körbersee: 20,812 pages / 101,978,800 bytes, SHA-256
`47c204e415b91add860a100533657854f162b9bee1702f3087775a6bd55d4ab6`.
Körbersee is exported in original cache order from prepared region request
`3aa47aef3fb86151fb3b0b6e8e45c0d710df14738ca3563202a5a1c415773a60`.
The input files remain external private evidence; they are not bundled in Git.

Failure tests use the real native device with injected rejection at record or
submission, plus actual R32 readback. They cover retry, changed existing pages,
old-page retention until complete terrain publication and atlas growth.
These checks complement complete public-client Places at 1920×1080/60, fresh
offline processes and the strict ten-second readiness limit. An isolated upload
improvement does not prove the complete loading, frame or Metal/A18 gates.

The original algorithm comparison considered individual uploads, submission-only
batching and packed staging on all 4,132 Flensburg pages. The packed variant won;
the implementation retains the existing R32/halo/codec contracts. The submission
and lifetime approach is informed by Khronos' SIGGRAPH 2019 timeline-semaphore
material, already reviewed and linked with the comparison in #441. This change
does not require timeline semaphores or change the native completion API.
