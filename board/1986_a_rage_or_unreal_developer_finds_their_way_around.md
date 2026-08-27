Type: issue
State: active
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

**A number in a name, and it is not a number.** `Plan2` (`generators/draw/BuildingShape.h`) is
`{double E, N}` -- an east/north point. The `2` reads as "the second Plan", and the tree already
has `Enu {E, N, U}` in `world/ground/tiles/TileGeodesy.h`. That is the no-primitive-twice rule
broken semantically, where the type-name claim cannot see it: two DIFFERENT words for one idea
rather than one word for two.

**Parts of speech that are not nouns.** `Quietly` is a `Sink` that collects numbers instead of
printing them; `ToTheClient` is a `Script::Host` that forwards calls. An adverb and a
preposition. Unreal would call the first an output device and the second a bridge; either way a
type is a thing.

**Body parts and tack.** `Footing` (a foot), `Reins` (a horse's), `Astride`, `Gait`, `Wings`.
`Reins` is `{SettleS, LeastReachM, TightestPerM, HoldWithinM, AsideM}` -- the bounds a pilot
holds a line within. A riding metaphor for a control law: vivid to whoever wrote it, opaque
to everyone else, and it names a subject the engine is not supposed to know.

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
- [ ] `TheEngineNamesNoSubject` walks CLAUDE.md too. It walks `src/` and `include/`, so the page
      that STATES the rule was the only place not held to it -- and carried three subject nouns
      until this item found them
