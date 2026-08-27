Type: issue
State: open
Parent: 1953
Area: architecture
Tags: naming, benchmark

# A RAGE or Unreal developer finds their way around this tree

**Benchmark** — Unreal and RAGE have each spent a decade settling what to CALL things, and the settlement is worth more than either implementation: `FScene`, `FPrimitiveSceneProxy`, `FViewport`, `FTransform`, `FTaskGraph`, `RenderTarget`, `Pass`; `fwEntity`, `grcDevice`, `phInst`, `Mat34V`, `sysTaskManager`. **Taking the ORIENTATION and not the spelling** — copying `F` prefixes would be cargo cult, but a name that means something ELSE in both engines costs a reader who knows one of them.

outshine is meant to be a reference design a technical university could teach from. That reader
is not a stranger to the field -- they have read one of these two engines. A name that collides
with an established meaning spends their knowledge against them.

**MEASURED, and the first one found is the shape of the rest.** `Engine::State::Picture.Canvas`
was a bool meaning "a render target stands". In Unreal, `FCanvas` is the 2D drawing context for
HUD and debug text -- so an Unreal reader reads `Canvas` and thinks *someone is drawing a HUD*.
The thing it names is a `FViewport` / render target. It is `Targeted` now, which says what it
measures and collides with nothing.

**This is NOT a licence to rename everything.** The tree's own voice -- verbs and participles
rather than noun soup, `Stands`, `Rides`, `Draws`, `Kept` -- is deliberate and reads well; it is
what makes a header sound like a sentence. What is in scope is a name that means something
DIFFERENT in Unreal or RAGE, because that is a trap rather than a style.

Where the tree already agrees without copying: `SubjectProxy` against `FPrimitiveSceneProxy`,
`Stage` against a render pass, `Store` against `UWorld`'s entity side, `Standing` against
`FTransform`, `Work::Graph` against `FTaskGraph`.

## The survey, by gut feel and then checked

Every class, struct and header name in `src/` and `include/`, sorted by how wrong it looks.
Nothing here is a style complaint -- each row is a name that costs a reader something.

**A number in a name, and it is not a number.** `Plan2` was `{double E, N}` -- an east/north
point -- and the `2` read as "the second Plan". It is **`En`** now, which sits beside `Enu` and
`Ecef` in the tree's own geodesy vocabulary: a reader who knows one reads the other without
being told.

Looking for a home for it turned up something larger, and it is NOT a rename. `src/base/math/Vec3.h`
declares no type at all -- it is free functions over `double[3]`, and the door passes raw arrays
everywhere. That is a deliberate choice (contiguous, one-width, pointer-free) and it is right for
a boundary. But `TreeVec3 {float X,Y,Z}` and the old `Plan2` were private exceptions to it, so
the tree has THREE conventions for a small vector where Unreal has `FVector` and RAGE has `Vec3V`.
Whether outshine should have one named vector type is an architectural question this item does
not get to settle by renaming things.

**Parts of speech that are not nouns.** `Quietly` was a `Sink` collecting numbers instead of
printing them, and `ToTheClient` a `Script::Host` forwarding calls -- an adverb and a
preposition. They are **`Collecting`** and **`Forwarding`**: participles, which is the tree's own
voice, and each says what the thing DOES rather than how it does it.

**Body parts and tack.** `Reins` was `{SettleS, LeastReachM, TightestPerM, HoldWithinM, AsideM}`
-- the bounds a pilot holds a line within. A riding metaphor for a control law: vivid to whoever
wrote it, opaque to everyone else, and it named a subject the engine is not supposed to know. It
is **`Holding`** now, beside the `Hold` it parameterises. Still open: `Footing` (a foot),
`Astride`, `Gait`, `Wings`.

**Collides with an established meaning.** `Live` is the renderer-side subject holder; in Unreal
"Live" is Live++ hot reload, so an Unreal reader expects recompilation. `Viewport` needs
checking against `FViewport`. `Transform` (`content/gltf/Transform.h`) sits beside `Standing`
in the door -- two names for a placement, which is board:1980's other half.

**Says nothing.** `Types`, `State`, `Kind`, `Where`, `Value`, `Table`, `Section`, `Page`,
`Fields`, `Making`, `Shows`, `Taken`, `Yield`, `Rank`, `Emit`, `Paint`, `Resolve`, `Unwired`,
`Reaped`. Some are fine inside a small namespace; `State` and `Types` as FILE names are not.

**What is right and should not move**, so the sweep does not become a rewrite: `SubjectProxy`
(Unreal's FPrimitiveSceneProxy), `Store` (UWorld's entity side), `Stage` (a render pass),
`Standing` (FTransform), `Work::Graph` (FTaskGraph), `Mixer`, `BusGraph`, `Ledger`, `Corridor`,
`Ribbon`, `ClusterDag`. The tree's verb-and-participle voice is deliberate and reads well.

- [ ] every type and member in `include/` is checked against what the word means in Unreal and
      in RAGE, and a collision is either renamed or the item says why the tree's meaning wins
- [ ] the same for the names a client SEES: measure strings, refusal texts, event names
- [ ] a claim holds the list of known collisions at zero, the way `TheEngineNamesNoSubject`
      holds subject nouns
**WITHDRAWN: `TheEngineNamesNoSubject` will NOT walk CLAUDE.md, and the reason is the measurement.**
The page carries `car` once, `seat` once, `wheel` once -- all three inside the sentence that
DEFINES them as subjects -- and `door` six times, every one of them the interface metaphor this
tree uses throughout ("a header is public only if a client cannot use the engine without it").

A word count cannot tell a page NAMING a thing from a page TALKING ABOUT the word, and CLAUDE.md
does nothing but talk about words. A guard there would fire on its own rule and be silenced within
a week. The three subject nouns it did carry -- "what a WHEEL stands on", "vehicle numbers", "the
corridor the wheels stand on" -- were found by reading, and reading is the right instrument for a
page that argues rather than declares.
