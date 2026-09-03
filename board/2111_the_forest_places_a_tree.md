# The forest places a tree

State: open

`Forest` is registered, is asked, and places NOTHING. Measured on 2026-09-03 through
`shots --measures`, after teaching the ledger to report per generator:

  OldTown      building placed 1275, flora placed 0
  Jura         building placed   32, flora placed 0
  Kaiserberg   building placed  740, flora placed 0

Not one tree, anywhere, including the place named for the forest it stands in. The catalogue
stands (`a shipped catalogue stands 1.000`), the region is leased, two makers are asked, and one
of them returns empty every time.

Everything downstream of this is unmeasurable while it holds. The generator RANKS -- which subject
takes ground before which -- cannot be tested, because only one subject ever takes any: reversing
flora and building changed 1275/1845 to 1275/1845 exactly. The 1845 refusals are buildings against
BUILDINGS, which OSM outlines do when a courtyard or an annex overlaps.

**Benchmark**: Unreal's procedural foliage spawner reports what it spawned and what it rejected per
spawner, and a spawner that yields nothing shows up as a zero the author sees. RAGE's prop
placement is authored, so an empty result is a content bug found in the editor. Neither ships a
placer that silently returns empty. Taken: the ledger reports per generator now, which is what made
this visible at all -- the summed number said 1275 bodies and looked healthy.

**Where to look:** `Shipping::Stands` refuses when the declaration names no species or no density,
and it did NOT refuse, so `Stems_` and `PerM2_` are non-empty. That leaves `Forest::Occupy`: either
the ground it is handed reports no cover it will grow on, or its density resolves to zero per
square metre, or every claim it makes lands outside the region it was leased. The last one is
already distinguishable -- `flora wanted ground off the region` is 0, so it is not even ASKING.

**The measurement that closes it:** `flora placed` above zero in Jura and Kaiserberg, and a picture
with trees in it. And then, for the first time, the ranks become testable: swap flora and building
and the two numbers must MOVE.
