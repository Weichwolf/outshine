# The corpus preparer

Offline preparation of the render ladder's subjects and their Blender oracle. This is the one door
`CLAUDE.md` leaves open for a script that is not C++: it prepares data offline and is committed
beside what it produces. **It is never a test, a gate, a build step, or anything at runtime**, and
nothing under `test/run.sh` or the Makefile reaches it.

It does three things, each independently invocable and each idempotent:

| | |
|---|---|
| **fetch** | a subject by pinned identity, verified against a sha256 the manifest records |
| **convert** | a `.blend` to glTF through Blender, when the subject is a `.blend` |
| **render** | the oracle: Cycles, EXR f32 for the score, raw f32 for the C++ runner. **No picture** — both pictures are the runner's, encoded from the two buffers it scores, so an image and the number taken from it cannot come from different sources |

It does **not** compare, score or decide anything. That is C++, in the test, where the harness's
printed trailer is the single verdict.

## Invocation

```sh
python3 test/corpus/prepare.py dry-run --manifest test/render/coverage/triangle/manifest.json
python3 test/corpus/prepare.py all     --manifest test/render/coverage/triangle/manifest.json
```

`fetch`, `convert`, `render` and `all` are the jobs; `dry-run` prints what a cold run would cost and
touches nothing. `--dest` overrides the destination, which is otherwise the manifest's own
directory; `--store` overrides the content store; `--recipe NAME` renders one recipe; `--force`
redoes the work over a cache hit; `--no-cache` skips the store in both directions.

Blender is taken from `--blender`, then `$OUTSHINE_BLENDER`, then `PATH`, then the two usual macOS
locations. It is **refused if absent** — the oracle is Blender, and a silent skip is the defect class
this repository keeps finding. Stdlib only on our side; `numpy` is used inside Blender, which ships it.

## What a test directory holds

```
test/render/<feature>/<case>/
    manifest.json           tracked -- the only tracked file
    scene.gltf              fetched, or scene.glb produced by the conversion
    0-reference.png         the runner's, RGBA, encoded from oracle.raw
    1-outshine.png          the runner's, RGBA, what we produce now
    oracle.exr              ours, f32, what the score is computed on
    oracle.raw              ours, flat f32, what the C++ runner reads
    provenance.json         ours, what actually ran
```

A recipe other than `default` names its products `oracle.<recipe>.exr` and `oracle.<recipe>.raw`.
`0-reference.png`, `1-outshine.png`, `outshine.exr`, `outshine.raw` and `provenance.json` are a
reserved set the preparer refuses to name, so a collision has no spelling here rather than a rule
against it.

**One alpha convention, on both sides: RGBA, straight, alpha is coverage.** Cycles writes it into the
EXR's fourth channel for camera rays that missed, and the runner carries it into both PNGs and into
the comparison. Without it a black subject and no subject are the same three channels — measured on
this corpus at 46 101 pixels of one case.

Everything but `manifest.json` is derived and untracked, under the single tracked
`test/render/.gitignore`. *Untracked* means not in git, not absent: the products stay in the folder
and regenerate in place, so an incremental run never leaves a directory image-less.

## The content store

The global one, `src/data/ContentStore.{h,cpp}`: `hash = filename`, a directory with no index and no
sidecar, under the host's temp directory as `outshine-content`. Writes land on a temporary and are
renamed, so a name never precedes its bytes. There is no second cache.

**A fetched file is keyed by its own sha256**, which is the pin, so the store verifies itself.

**A derived artefact is keyed by a recipe hash** built the way `Data::ContentKey` builds one:
newline-separated, with a derivation version inside. What the key covers:

| | |
|---|---|
| the declared Blender version | a manifest bump on an unchanged host must miss |
| the observed Blender version and build hash | a host that moved under an unchanged manifest must miss |
| every subject's file digests, and the converted glTF's digest | |
| the whole declared scene — camera, light, world, material | |
| the whole render recipe | |
| the product — `exr`, `raw` — separately | |

Both Blender versions are in the key and neither alone is enough. This is not about reproducing
pixels; we do not aim to be bit-identical with Cycles. It stops one real defect: the pin is bumped,
scene and recipe unchanged, and the cache hands back a render from the old Blender while the manifest
claims the new one — the manifest lying about its own output.

A version **difference** is recorded in `provenance.json` and printed as a notice. It is never a
refusal. A **scene** sha256 mismatch is always a refusal.

## `oracle.raw`, the flat f32 dump

C++ has no EXR reader, SDL3 provides none, and vendoring OpenEXR to compute an IoU would buy nothing.
So the same pixels are written a second time in a form a reader can be twenty lines long for.

```
offset  type      value
 0      char[8]   "OSRAWF32"
 8      u32       0x01020304, written natively -- a reader compares it to its own to learn the order
12      u32       version, currently 1
16      u32       width
20      u32       height
24      u32       channels
28      u32       headerBytes, the offset of the first sample; a multiple of 4
32      u32       rowOrder, 0 = the first row in the file is the top row of the image
36      char[]    channel names, one per channel, each NUL-terminated: "R\0G\0B\0A\0"
        NUL       padding to headerBytes
headerBytes       f32 samples, row-major, channel-interleaved, no compression
```

Scene-referred linear, the same values the EXR carries — the view transform is not applied to either,
by Blender's own colour-management rule, which is what deletes AgX in one move. 1280×720 RGBA is
14 745 644 bytes.

The samples come back through the EXR rather than through `Render Result`, which refuses pixel access
in background mode.

## The manifest

`manifest.json` beside the products. **An unknown key is a refusal, not a shrug** — a key nobody reads
is a setting that silently did not apply, and that is the whole failure class this file exists to
close. Every refusal names the subject, what was expected and what was observed.

### Why each field is there

| Field | Why |
|---|---|
| `schema`, `schemaVersion` | a manifest written against a different schema is refused rather than half-read |
| `id`, `title`, `covers` | `covers` names the requirement identifiers, so a test can say what it holds |
| `criterion` | **what correct IS, in the asset's own words, with the file those words came from.** `kind` is `numeric`, `self-describing` or `limits-probe` and the runner's instrument follows from it, so a case cannot move from a number to an eye without a quotation moving with it |
| `subjects[]` | **a list, not one subject**: rung 21 is a scene plus a character plus our camera path, and a schema that assumes one subject would have to be rewritten to say so |
| `subjects[].id` | distinct, and it labels every row of the report |
| `subjects[].kind` | `gltf` or `blend` — the enumeration decides whether the conversion job runs |
| `subjects[].source` | `khronos-sample-assets` carries the commit, the model and *why the pin is where it is*; the plain-URL kinds carry the `page` that states the licence, because a plain URL carries no metadata |
| `subjects[].files[]` | one entry per file: `url`, `sha256`, `bytes`, the name it lands under, its role, its licence. `member` names a path inside a ZIP, and then the `sha256` is the **member's**, not the archive's |
| `subjects[].files[].licence` | **per file, never per repository.** A repository-level claim covers nothing in particular. A file with no licence field is refused at parse |
| `subjects[].entry` | which of the declared files is the glTF or the `.blend` |
| `subjects[].conversion` | required exactly when the subject is a `.blend`. Every export setting is written out, none left to a default |
| `blender.version` | a release version, not a commit. It is what a red rung is attributed with, and it is in the oracle's cache key |
| `scene.frame` | `gltf` — right-handed, +Y up, metres. A vector without its frame means whatever the reader assumes |
| `scene.camera` | `source: manifest` declares position, look-at, roll, yfov, sensor height and clip range; `source: gltf` adopts the file's own camera and refuses if there is not exactly one |
| `scene.light` | `none`, `sun` (irradiance in W/m² perpendicular to the beam, which is exactly Blender's Sun Strength) or `point` (watts and radius, the factory lamp) |
| `scene.world` | `factory` leaves Blender's world exactly where it is and records what it observed; `uniform` states a colour and a strength |
| `scene.material` | `source: manifest` replaces every imported material; `source: gltf` keeps them; `source: gltf-base-colour` keeps what the importer wired into each Principled BSDF's **Base Color** — factor times texture — and replaces only the closure. `kind` names that closure: `diffuse` is a Diffuse BSDF at roughness 0 and holds the closed form `rho*L` only where no surface can see another; `emission` is an emitter at strength 1 and is what a subject that shades itself must declare. Where the glTF material is not `doubleSided`, a back-facing hit becomes a Transparent BSDF, because **Cycles has no back-face culling for camera rays** and the format's own rule has to be expressed inside the oracle rather than tolerated outside it |
| `renders` | a **map** of recipe name to recipe, because rung 1 needs two renders of one scene: a binary mask and an alpha coverage. `default` must exist and is what the acceptance numbers are judged on |
| `acceptance` | **stated before the run and read from here by the test**, so a number cannot be edited to match a result it failed |

### Numbers carry their origin

Every acceptance entry is an object, never a bare float:

```json
"boundaryP95MaxPx": { "value": 0.5, "unit": "px", "origin": "SET", "note": "..." }
```

`origin` is `SET`, `derived` or `measured`, and a `derived` number without its `derivation` is
refused. A bare float has no spelling in this file — the shape carries the rule rather than a
checker counting it.

### What is refused, and stays refused

`licence.py` holds the allow-list — `CC0-1.0`, `CC-BY-4.0` and the `LicenseRef-LegalMark-*` marks —
and a table of named subjects with the reason each is out: Sponza (Crytek EULA), BrainStem (Poser
EULA), DamagedHelmet (CC BY-NC), BoxTextured (trademark), Duck (SCEA), and the rest of Khronos's own
`Models-issues.md` list. A refusal on paper is a habit; a refusal in the table is a rule.

For a Khronos subject the licence is **derived, never transcribed**: the model's `metadata.json` is
fetched at the pin, its `legal` array is read, and the manifest's declared set must equal it. A
licence change upstream is then a refusal rather than a surprise.

### Where bytes may come from

`fetch.py` holds the whole of it: the pinned `glTF-Sample-Assets` tree at
`raw.githubusercontent.com`, `download.blender.org/demo/{cycles,eevee,bbb,asset-bundles}/`, and
`studio.blender.org`. A redirect off the list is refused too. Anything else is not a source: it has
no pin, no stated licence and no reason to be trusted.

Blender Studio and Blender demo archives run to hundreds of megabytes, so a ZIP is **read, never
downloaded**: the end-of-central-directory record comes out of the last 64 KiB by HTTP range, the
central directory follows, and a member is fetched by the byte range its local header points at.
Measured on `download.blender.org/demo/bbb/blender.zip`: 830 709 844 bytes, 598 members listed in
0.34 s, one 140 KB member extracted in 0.27 s.

## The six settings that are load-bearing in a conversion

`export_apply` on against its `False` default, and exclusive with shape keys · `export_cameras` on
against its `False` default, because a downloaded scene's `.blend` is authoritative for its camera ·
`export_lights` **off**, so the light never crosses and a downloaded scene is judgeable on coverage
and depth until its rig is re-declared beside it · `export_animation_mode` `SCENE` ·
`export_optimize_animation_size` **off** against its `True` default, so the glTF's input accessor is
the frame grid · `frameStart` 0, because the exporter divides the absolute frame number by fps and
subtracts no start frame.

`export_format` must be `GLB`. A `GLTF_SEPARATE` export writes a `.bin` and a texture directory
beside the `.gltf`, and a store keyed on the `.gltf` alone loses them — measured, the import then
fails on a missing `scene.bin`. One file per conversion, one hash, one name.

A setting the exporter does not have is a **refusal**: a conversion that silently ignores one is a
conversion nobody can attribute afterwards.
