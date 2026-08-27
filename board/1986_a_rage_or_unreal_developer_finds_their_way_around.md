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

- [ ] every type and member in `include/` is checked against what the word means in Unreal and
      in RAGE, and a collision is either renamed or the item says why the tree's meaning wins
- [ ] the same for the names a client SEES: measure strings, refusal texts, event names
- [ ] a claim holds the list of known collisions at zero, the way `TheEngineNamesNoSubject`
      holds subject nouns
