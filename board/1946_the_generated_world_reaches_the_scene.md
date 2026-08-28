Type: feature
State: open
Progress: streaming
Area: world
Tags: benchmark, target

# What the generators build reaches the scene a scenario declares

**Benchmark** — Unreal: PCG output becomes actors in the same world. RAGE: generated content ships in the map. **Both agree** — what a generator builds reaches the scene through the same path authored content does.

Both benchmarks are world-streaming engines first and everything else second: Unreal's World
Partition streams cells with their actors, RAGE streams its map by node. outshine has the
generators and no path from them to a picture.

Measured: `src/world/generators/` plus `draw/`, `RegionForge` and `Clients::Sim` are 6528 of the
tree's 49769 lines -- buildings, tree growers with leaf-angle distributions, forests, roof
surfaces, water, infrastructure. `RegionForge` is held only by `Clients::Sim`, and
`grep -rn '"Sim.h"'` finds exactly ONE line: its own `.cpp`. Nothing else in the tree includes it.

- [ ] `Clients::Sim` has a consumer, or its capability moves to something that does (board:1805)
- [ ] `Stage::Terrain`, `Stage::Buildings` and `Stage::Water` execute, so what a generator makes
      can land in a pass (board:1805)
- [ ] a generator is a LIBRARY emitting the representation the compositor consumes, rather than
      a program with its own loop (board:1197)
- [ ] a building, a tree and a water surface stand beside the road in the still (board:1936)
- [ ] the world composed for a drive and the world composed for a viewer are ONE path
      (board:1805)
- [ ] **the world a drive moves through is re-composed INCREMENTALLY, not rebuilt per step.**
      Moved here from board:1574, whose own measurement sent it: that item blamed
      `SubjectDraw::SetMesh` for re-uploading every stream at every relay, and a `mesh-relay` tag
      over a 48-step drive reads ZERO -- the renderer costs nothing per step. What the drive
      takes, per step, over `apps/driver/src/f31.scenario` at 320x180:

          world-restand         1 106 280 B      GroundStack::Restand
          untagged              1 365 596 B      a thread no tag covers yet
          world-grow               60 726 B
          frame-tells               3 484 B
          render-frame              1 328 B
          mesh-relay                    0        the cause board:1574 named

      Unreal's World Partition streams a CELL in and out and does not rebuild the ones that did
      not change; RAGE streams by map node with the same rule. **Both agree**, and 1.1 MB a step
      for a drive that moves a few metres is a rebuild wearing a stream's name.
      The instrument is `heap taken under <tag>` through the door, differenced by the consumer;
      `apps/bench --steps 48 --heap --cache <store>` is the reading above, verbatim.
- [ ] a route crosses a continent over a graph that STREAMS rather than one held whole
      (board:1503)
- [ ] cut and fill meet the road, and the road is drawn (board:1505)

## MEASURED: the generators say WHAT each instance is, and the engine's sink drops it

Five places on Earth now fetch their terrain and their OSM, and the numbers are the places
themselves -- Shibuya 15 833 streets and 91 208 footprints, Rothenburg 2 633 and 6 423, the canyon
rim mostly trails. The pictures in `build/places/` show terrain and **not one building or street**.

The chain is: OSM -> `BuildingField` and `StreetField` -> `Generators::SnapshotOver` ->
`Shipping::Drawing().Draw(...)` -> a `DrawSink` -> `World.Instances`. It ends there:
`grep -rn 'World.Instances' src/` finds the fill and the SIZE being published, and no third reader.

And the reason nothing downstream *could* draw them is one line in `src/engine/Picturing.cpp`:

    [[nodiscard]] bool Add(Generators::BodyId, Generators::ClusterId,
                           const Generators::Instance &instance) noexcept override {
      Into_->push_back(instance);
      ...

**`BodyId` and `ClusterId` are unnamed parameters.** The generator states which body and which
cluster every instance is, and the engine's sink throws both away and keeps the bare transform:

    struct Instance { float Em, Nm; float AslM; float YawRad; float Scale; };

So `World.Instances` is a list of places with no subjects. Nothing can draw it because nothing in
it says what to draw. `ClusterId` is declared in the same header, beside `Instance`, and `Instance`
does not carry one.

Unreal: `FInstancedStaticMeshComponent` holds a MESH and its per-instance transforms together, and
`FGPUScene` keeps the primitive id beside every instance. RAGE: an entity carries its drawable
dictionary index. **Both keep the identity with the transform**, because a transform alone cannot
be drawn.

- [x] the sink keeps the body and the cluster it is handed, so an instance says what it is.
      `Surrounds::Standing` carries `{Body, Cluster, Where}` and `Instancing::Add` names both
      parameters instead of dropping them. Necessary and NOT sufficient, which the item's own
      control said before the work: the pictures are unchanged because nothing reads the list at
      all. proof: outshine/places 5 PASS, gate GREEN -- and the picture is the control
- [ ] the instances reach the picture through the same instanced draw a body uses -- one call, N
      instances, N rows (landed in board:1574)
- [ ] a place's footprints appear in `build/places/`, judged by eye against the written expectation

**The measurement that would show I am wrong:** if the ids are kept and the pictures still show no
buildings, the sink was not the blocker and the fault is downstream in the draw sources. The count
to watch is `instances its draw sources made` against what a place HOLDS -- Shibuya makes 4 096
against 91 208 footprints, which is the vegetation budget rather than the buildings, so those two
numbers should converge on the same subject when this is right.

## And the measurement split one gap into two

Keeping the ids changed no picture, which is what this item wrote down beforehand. What it also
showed is that the remaining work is TWO things rather than one:

**One, nothing draws the list.** `World.Instances` has a writer and a size-reader and no third
reader. Drawing it is board:1574's instanced draw -- one call, N instances, N rows -- pointed at a
geometry the body id names.

**Two, and this is the larger half, now measured to the bottom: NOTHING PLACES A BUILDING.**

    Forest::Proposes          proposes -- the only one
    Buildings::Proposes       return 0
    Water::Proposes           return 0
    Infrastructure::Proposes  return 0

And `Generators::Shipping::Stands` builds exactly one thing: a `Forest` and its `ForestDraw`. So
every one of the 203 bodies at Rothenburg and 4 096 at Shibuya is a TREE, and the owner's
instruction for stage two is *without vegetation* -- the one generator that places is the one not
wanted.

`Buildings::Occupy` walks the footprints, calls `yield.Count(Footprints)`, raises a note about the
highest roof, and asks for nothing. The footprints are read as GROUND TRUTH for occupancy and roof
height, never as subjects. That is not a defect to fix but a capability that does not exist, and
saying so is worth more than another measurement: `BuildingMesh` and `BuildingShape` are written
and reached only by `Structures::Make`, which answers an `Ask` for one object rather than placing a
field of them.

**The footprints never BECOME instances.** Rothenburg holds 6 423
footprints and the generators place **203** things. Shibuya holds 91 208 and places 4 096, which is
`kMostInstances`-shaped rather than building-shaped. So the draw sources are placing vegetation and
structures from their own tables, and the OSM footprints are handed in as a FIELD that something
must still turn into bodies. Drawing the list would show trees at a place whose buildings are still
absent.

The number that closes this: `instances its draw sources made` and `building footprints it holds`
converge on the same subject. Today they are 203 against 6 423 and they are not even about the same
thing.


## THE BUILDINGS ARE MESHED, PLACED AND DRAWN -- AND STILL NOT SEEN

Measured at Rothenburg, 400 m above the ground, sun declared at 60 deg:

    building triangles meshed from OSM footprints   622 596
    triangles reaching the renderer                 840 396   (of which the ring is 217 800)
    buildings stand between                         329 and 489 m up
    their vertices lie                              1 to 3 167 m out
    the ring's nearest vertex                       11 m out at 432 m up
    the eye                                         830 m up

Every one of those numbers is right. Rothenburg's ground is about 430 m, its valley about 330, and a
church tower reaches 489. The buildings are inside the frustum, in front of the eye, above the
terrain under it.

**THE CONTROL THAT PROVED IT.** Lifted 500 m, the whole town appears -- and its shadows fall on the
ground below with Rothenburg's street plan, its ring road and its market square legible in them. So
the geometry is correct, the placement is correct, the winding is correct, the shadow pass sees
them, and the material reaches them.

At their true height they are two red pixels when painted pure red. At +20 m, painted red, nothing.
At +500 m, a town. The transition sits somewhere between 20 and 500 m, which no number here
predicts, and that gap is the finding rather than any of the causes ruled out:

- NOT the winding: matched to the ring's, which is swapped for this renderer
- NOT the material: given their own surface, and pure red shows the same two pixels
- NOT the placement: east/north/up all measured and all correct
- NOT missing from the draw: 622 596 of the 840 396 triangles submitted are theirs

## What this item still owes

- [ ] the buildings are visible at their own height, and the number that says so is the transition itself -- at what offset do they emerge, and why is it not zero
- [ ] whatever the cause, its negative control is the +500 m lift: it must stop being necessary


## THE BUILDINGS DRAW. SHIBUYA'S SKYLINE STANDS.

Tokyo's towers are in `build/places/Shibuya.png` -- the tall spire among them -- so the whole path
works: OSM footprint, mesh, part, surface, placement, draw, frame. The question is no longer whether
they draw.

## EIGHT CANDIDATES DEAD, EACH BY A NUMBER

    a refused part            all three Geometry calls report taken; part 1, surface 1, 2 parts
    scale                     60 m above ground, where a 12 m building at 100 m covers 45 px
    the material              own surface; pure red shows the same
    lighting and shadow       EMISSIVE at 40 000 -- neither can hide it
    the winding               matched to the ring's, which is swapped for this renderer
    the anchor                0 m from the render frame's origin
    an unwritten placement    both parts read identity: sum of absolute terms 4, diagonal 4
    part ranges               part 1 first vertex 653 400, count 1 867 788, indices offset to match

    the ring within 3.2 km    318 to 460 m up over 150 738 vertices
    the buildings             329 to 489 m up, 1 to 3 167 m out
    the eye                   490 m up, ground 430

The buildings sit INSIDE the ring's own range and their tops reach 29 m above its highest point. So
they are not buried.

## WHAT IS LEFT, stated as the pattern the numbers show

**Far buildings draw and near ones do not.** Shibuya's towers stand at kilometres while its 91 208
footprints leave the near field empty; Rothenburg's 5 499 leave it empty at every distance and its
tallest are 60 m rather than 250. So what separates a drawn building from an absent one is SIZE ON
SCREEN or DISTANCE, not placement, material, lighting or topology -- every one of those is dead
above.

## The measurements that would settle it

1. **The near plane.** `SubjectProxy` takes `eye.ZNearM` when it is positive and `Renderer::kNearM`
   (0.05 m) otherwise. With a ring spanning 388 km a framing-derived near plane could be hundreds of
   metres, and everything closer would be clipped. Publish it: if it is 0.05 m this dies
2. **A count, not an impression.** How many building triangles survive to the draw against the
   622 596 submitted -- if the draw receives all of them, the loss is in the pipeline and not in the
   list
3. **The negative control is Shibuya itself.** Whatever the answer, it must leave the skyline
   standing and put the near buildings beside it
