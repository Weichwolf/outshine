Type: feature
Area: render
Tags: oracle, khronos, perf

**I.26.10 A render test is a directory, and in the minimal case that directory is one `.gltf`**

*Owner's ruling, 2026-08-12: **the render tests are declarative. The glTF is the declaration. One runner
renders oracle and outshine and scores them.** Taken to its strongest form: **a test is a directory and
the directory is self-describing.** It supersedes the earlier "one directory with its Blender script and
its expected values". `<area>` still mirrors `src/` as always.*

*Superseded 2026-08-12, second time, and the trade is written out because the reason the first form was
chosen is still true: this section's own first draft required a `manifest.json` per case, that draft was
struck because it made adding a case a writing task, and the strike went one step too far — **`case.json`
optional plus a global corpus manifest put a case's subject, its licence and its comparison in three
files**, which is the split the last line of this section already calls out. The merged form keeps the
cheap-case property by making the manifest a **delta over declared defaults**, not by making it absent:
a tracked-`.gltf` case with the derived camera and the default recipe is `schema`, `schemaVersion`, `id`
and `covers` — four lines. The 190-line one under `render/coverage/triangle/` is long because it is a
**fetched** case with a **declared** camera and **two** recipes, and every one of those is a deviation
that has to be written down somewhere.*

```
test/khronos/glTF/<feature>/<case>/
    manifest.json     tracked              THE DECLARATION — the only tracked file
    scene.gltf        tracked or fetched   the subject
    0-reference.png   always written       for the eye, in browsing order
    1-outshine.png    always written       for the eye
    oracle.exr        always written       float32 — this is what the score reads
    oracle.raw        always written       float32, flat — what the C++ runner reads
    outshine.exr      always written       float32
    provenance.json   always written       what actually ran
```

- [ ] **A four-line `manifest.json` beside a `.gltf` is a complete, valid, scoring test case** — camera, thresholds and render recipe all resolve from declared defaults, and every field beyond those four exists only to override. *The writing cost the first strike was protecting is preserved and the measure of it is the diff: if the minimal case is ever more than the schema, the id and what it covers, this line has been broken*
- [ ] **A recipe other than `default` names its products `oracle.<recipe>.exr`, `0-reference.<recipe>.png` and `oracle.<recipe>.raw`**, so the eye's files still sort to the top, and `1-outshine.png`, `outshine.exr`, `outshine.raw` and `provenance.json` are a **reserved set the preparer refuses to name** — a collision has no spelling rather than a rule against it
- [ ] **`oracle.raw` is a flat float32 dump beside the EXR, because C++ has no EXR reader**, SDL3 provides none, and vendoring OpenEXR to compute an IoU would buy nothing. Header: `"OSRAWF32"` · a natively-written `0x01020304` a reader compares against its own to learn the byte order · version · width · height · channels · header length · row order · NUL-terminated channel names; then row-major channel-interleaved samples, uncompressed. Scene-referred linear, the same values the EXR carries. 1280×720 RGBA is **14 745 644 B**. *The samples come back through the EXR rather than through `Render Result`, which refuses pixel access in background mode*
- [x] **The runner is one program over a directory** — read the glTF, resolve camera and recipe, render both sides, compute the named metrics, print a verdict. Still one process and one real verdict per case (§ I.20); what is shared is the code, never the process *(`test/shared/render/Parity.cpp`, `test/outshine/render/triangle/`)*
- [ ] **The camera comes from the glTF wherever the scene declares one**, which is what makes such a scene fully self-describing — glTF carries `cameras` and a node that references one. A declared camera is used verbatim and no framing rule runs
- [ ] **Where the scene declares no camera, framing is derived by one rule stated here and never per test**, so two cases with the same subject get the same picture and nobody tunes a viewpoint into a pass

**The framing rule, and it is deterministic by construction rather than by care.**

| Step | Rule |
|---|---|
| bounds | world-space AABB over every rendered primitive, node transforms applied |
| centre | `c = (min + max) / 2` — **from min/max only, never a vertex mean** |
| radius | `r = ‖max − min‖ / 2`, the bounding sphere's radius, so the framing does not change with view direction |
| direction | fixed unit vector in glTF's +Y-up frame: **azimuth 35°, elevation 20°**, `[SET]` — off every axis, so no face is edge-on and no silhouette is degenerate |
| distance | `d = r / sin(yfov / 2) / fill`, with `yfov` **39.6°** and `fill` **0.6** `[SET]` — the subject's bounding sphere spans 60 % of the frame's vertical extent |
| clip | `znear = max(d − r, r/1000)`, `zfar = d + r` |

- [ ] **Load order cannot move that camera, and the reason is arithmetic rather than discipline**: `min` and `max` are exact in IEEE-754 and both commutative and associative, so the AABB is identical whatever order primitives arrive in. **A centroid would not be** — floating-point addition is not associative, so a vertex mean shifts in the last bits when the loader changes, and every boundary-displacement number in the suite would shift with it. That is why the centre is defined off the extremes and the rule says so
- [ ] The framing rule's constants live in the const headers of § I.23 like every other number — one declaration, no per-test copy, and changing one moves every derived-camera case at once, which is the intended blast radius
- [x] **Degenerate bounds are a refusal, not a fallback**: an empty AABB (no rendered primitive) or `r = 0` (every vertex coincident) refuses by name. A fallback camera here would manufacture exactly the empty picture § I.26.11 exists to catch *(`src/gltf/Subject.cpp` `Frame`, `test/shared/render/Parity.cpp`)*
- [ ] **Thresholds default per instrument class** (§ I.26 — opaque ≥ 1 px, sub-pixel present) and a case overrides only where it earns it, with the reason in its `manifest.json`
- [ ] **A threshold restated in a manifest rather than inherited is a defect, even when the value it restates is right today.** `render/coverage/triangle/manifest.json` copies rung 1's boundary p95 as `0.5` and cites § I.26 for it, four months after § I.26 tightened that number to `0.1` in the same file — so the copy is a **stale quotation that reads as an authority** (the bug tasks in `board/`). Right: a manifest's `acceptance` block carries only what it **overrides**, with the reason, and an entry equal to the default is refused at parse rather than accepted as agreement
- [ ] **The default is not weaker against tampering than 200 scattered numbers — it is much stronger, and this is worth stating because it looks like the opposite.** One declared default in one file, read by every case, is **visible when edited**: the diff is one line and it moves the whole suite at once, so nobody quietly relaxes case 137. Two hundred per-case numbers are two hundred places a threshold can be nudged to match a result and have it read as ordinary work. *`CLAUDE.md`'s "a number stated before and after that nobody moves" is served better by one number than by two hundred*
- [ ] **The render recipe defaults the same way** — engine, device, samples, adaptive off, denoising off, seed, `diffuse_bounces`, pixel filter and width, resolution, output format — declared once in § I.26 and overridden per case only where the rung needs it, as rung 9 needs bounces on
- [ ] **The Blender release is part of the oracle cache key and not of any case file** (§ I.26), so the version cannot drift per case and the cache cannot serve a render the recipe no longer describes
- [ ] What `manifest.json` may carry beyond its four required fields: the **subjects** and their pins and per-file licences where the `.gltf` is fetched (§ I.26.1) · threshold overrides with their reason · a recipe override with its reason, or a **map** of named recipes where one scene needs two renders, as rung 1 needs a binary mask and an alpha coverage · an explicit camera where the glTF has none and the derived one is wrong for the question · the **requirement identifiers the case covers** (§ I.20) · and a **closed-form expected value** where one exists, because rung 3's `ρ·E·cos θ / π` is a value and a test that only checks a difference cannot tell a right answer from two wrong ones that agree
- [x] **Every acceptance entry is an object carrying its origin, never a bare float** — `{"value": …, "unit": …, "origin": "SET" | "derived" | "measured"}`, and a `derived` number without its `derivation` is a refusal. *A bare float has no spelling in the file, so `CLAUDE.md`'s "every number carries its origin" is carried by the shape rather than counted by a checker* *(`test/shared/render/Acceptance.h` `ReadDeclaredNumber`, `test/outshine/render/triangle/`)*
- [ ] **The acceptance is stated before the run and read from there by the test**, so a number cannot be edited to match a result it failed
- [ ] **The reference is derived, never committed** — 720p RGBA float32 is `1280·720·4·4 B = 14.75 MB` a frame, and a 240-frame film segment is **3.54 GB** on its own. What is tracked is the declaration; the pixels come from it and cache by hash
**Both pictures always land in the case directory, and that is where a human goes to look.**
*Owner's ruling, 2026-08-12: **the reference and outshine images are always placed in the test folder
containing the glTF, so I can see the progress.** It supersedes this section's earlier "ours lands
beside it", which left the writing optional and the naming unstated.*

- [x] **Both images are written on every run, pass and fail alike.** *A picture that only appears on a failure cannot show progress, which is the stated purpose — and a suite that shows nothing while it is green is a suite whose improvement nobody can see between two reds* *(`test/shared/render/Parity.cpp`)*
- [ ] **PNG always, and the float pair always beside it**: the EXR float32 pair is what every metric reads and what settles a radiance question; the PNGs exist **purely so the directory can be opened and looked at**. Neither replaces the other and neither is optional
- [x] **The names sort for browsing and the order is part of the contract**, not alphabetical accident: `0-reference.png` · `1-outshine.png` and **nothing else**, beside `scene.gltf` and the two `.exr`. The purpose is visual comparison in a file browser, so *reference first, ours second* is what the numbering buys. **Two images and no third**: a difference image is a picture of an arithmetic operation, it is read as evidence when it is only a restatement, and the pair is what a person actually judges *(`test/shared/render/Parity.cpp`, `test/outshine/render/triangle/`)*
- [ ] **Untracked but permanent — and the distinction is the whole line.** ~200 cases at 720p is **≈ 100 MB churning per run**, so the images stay out of git; *untracked* means **not in git**, never **not there**. **A cache hit must still materialise the oracle into the directory**, and an incremental run must never leave a case image-less — a directory with a stale picture, or none, is worse than one with no pictures at all, because it is silently wrong about what it shows
- [ ] **Motion cases get a numbered frame sequence plus one encoded file for the eye**, under the same rule: the sequence decides, the encode is for looking, and both are always written
- [ ] **This is where the by-eye judgement lives, and saying so makes it a property of the layout instead of a habit.** `CLAUDE.md` holds that appearance is judged by eye and in motion and that a number never decides whether it looks right — but nothing until now said *where a person goes to do that*. **It is this directory.** A scenario case (§ I.26.9) whose verdict is a frame-time distribution writes its pictures the same way and for the same reason, because its acceptance is half by eye and that half needs a home too
- [ ] A comparison whose halves live in different places is one somebody assembles by hand every time, and nobody does it twice
- [ ] **EXR float32 decides and PNG is for looking**, and both are written: a PNG carries a transfer function and cannot settle a radiance question. `diff.png` is a viewing aid and no acceptance reads it
- [ ] **The case declares both jobs in one file and that is the ruling, not a compromise** — *what may be fetched and under what licence* and *what this comparison is*. *Superseded 2026-08-12: the earlier line split them across a corpus manifest and a case file, so a URL and the acceptance it produces sat in two places that nothing kept in step, and a case could name a corpus id that had quietly moved.* What survives the merge, and it is the whole of what the split was buying: the **allow-list and the licence table are code, not manifest** (`fetch.py`, `licence.py`) — a manifest can name a URL, and a URL off the allow-list or a licence off the allow-list is a refusal it cannot talk its way out of
- [ ] **`provenance.json` records what actually ran** beside what was declared — the observed Blender version and build hash against the declared one, the store's hits, misses and writes, and the world colour and strength Blender was observed to have. A **version difference is a printed notice and never a refusal**; a **scene SHA-256 mismatch is always a refusal**. *That asymmetry is the owner's no-bit-identity ruling made operational: the oracle's version is attribution, the subject's bytes are the measurement*

**The verdict is named metrics, never one blended score.**

- [x] **Each metric carries its own threshold and its own direction, and pass is every metric within its own** — `boundary_p95_px` passes **at most** 0.1, `iou` passes **at least** its floor, `radiance_median_rel` passes at most 0.01, `coverage_fraction` passes at least its minimum. *A single blended score would be declarative and still wrong: it hides which metric failed, and a red nobody can attribute is a red nobody acts on* *(`test/shared/render/Metric.h`, `test/shared/render/Parity.cpp`)*
- [x] **Direction is an enumeration and never a boolean or a sign convention** (`Enum.2`) — `AtMost` · `AtLeast` — because *lower is better* encoded as a flag is the class of mistake that inverts a whole suite silently and passes every test while doing it *(`test/shared/render/Metric.h` `Direction`)*
- [x] The runner prints **every metric with its value, its threshold and its direction**, not only the failing one, so a case that passes narrowly is visible before it starts failing *(`test/shared/render/Parity.cpp`, `test/outshine/render/triangle/`)*
