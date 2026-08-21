Type: bug
Area: assets
Tags: bug, scope
Depends: 1543

**The declared F31 has a ROUTE INTO THE TREE**

`tools/driver/f31.scenario` declares `scene.gltf` by digest, and nothing in the repository could obtain
it. **`python3 test/harness/shared/corpus/prepare.py scenario-assets` now does**, and the windowed
driver stands the car beside the road it drives.

## What the step does, and why it is not a fetch

A Khronos subject carries a URL and is fetched. This one carries a **page a person visits and accepts
a licence on** -- `https://sketchfab.com/3d-models/2014-bmw-3-series-f31-71746440f98d48ca9ea41ceeaa3504c7`,
DisneyCars, CC-BY-4.0 -- so there is nothing a script may fetch unattended, and inventing a mirror URL
would be guessing.

**What a script CAN do is verify.** The scenario pins both digests, so the step searches the declared
roots, checks what it finds, and either places the declared asset **byte for byte** or refuses by name
with the source, the licence and the credit line. It never substitutes.

```
scene.gltf  c60068fcd0f8c25e73225cd3725a422fca46c00a2a68ca481988a6680cc5fb1d   matched
scene.bin   be46e9c11f5b7f16a2cc01a3a96b92394bff04ed3742a8974de2f9bc093ba453   matched
```

## What it took to put it in the picture

| | |
|---|---|
| `Gltf::Subject::Append` | joins geometry to geometry, padding each attribute run in both directions so a subject with uv and one without merge without desyncing |
| `Live::Build` | **read the file, then append the built corridor** as further parts, with its declared surface added to the table as one more slot. `PartSlot` already decouples a part from a document's material index, so nothing had to be renumbered |
| `Live::Carry(model)` | the parts before the join stand at the body's pose, the rest where the world put them |
| the picture | **259 parts -- 258 the car's, 1 the road -- and 942 841 triangles** |

## Comments

**THE REFUSAL THIS ITEM WAS FILED AS WAS WRONG, and the way it was wrong is worth keeping.** It said
the asset *cannot be fetched* on the evidence that the tree, the 1184 prepared cases, the content
store and `prepare.py` all lacked it. Every one of those was true. The conclusion was not: the asset
sat in `~/Downloads/2014_bmw_3_series_f31`, both digests matching. **Four searches inside the
repository were read as a fact about the machine.** An instrument's domain is part of its claim, and
that domain was the tree.

**And carrying the body through `Submit` cost 14.6x.** `Live::Carry` first called `Submit`, which
takes the `Move` path, which re-streams every vertex -- **942 000 of them, every frame, to change
sixteen doubles**. 19 999 frames took 543 s. Pushing only the placement takes 37 s for the same
frames. The frame times *rose* when it was fixed, 0.165 ms to 0.825 ms at p50, because the upload had
been sitting outside the timed region and the fence now waits on real work: **the honest number is the
larger one.**
