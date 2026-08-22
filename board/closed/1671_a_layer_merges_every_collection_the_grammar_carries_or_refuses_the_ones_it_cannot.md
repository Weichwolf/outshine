Type: issue
Area: scenario
Tags: mods, layering, loud-failure

**A layer merges every collection the grammar carries — or refuses the ones it cannot**

`MergeLayer` (src/scenario/ScenarioLayer.cpp:61-74) merges exactly FOUR of the Scenario
struct's collections: Kinds, Instances, Assets, Vehicles. The Scenario carries ~20
(include/outshine/Scenario.h:291-324): Regions, Doors, Volumes, Sounds, Buses, Tables,
Events, Views, Placements, Surfaces, Providers, Generators, Compositors, plus the
singletons Ground, Lit, Render, Motion, Time, Played, Driven, Input, State.

A layer that declares ANY of the other sixteen merges "successfully" and the declaration
VANISHES — the winter mod that dims `<lighting>` or adds a `<sound>` is exactly the
canonical mod, and today it is a silent no-op. This is the same silent-drop pattern 1667
just killed in the trait merge (`(void)resolved.Put(...)`), reborn one level up. "A
failure is loud" is a house rule; a merge that discards a parsed declaration without a
word is the opposite.

Demand, in order:

1. **Refuse first**: until a collection has a decided identity, `MergeLayer` refuses a
   layer whose unmerged fields are non-empty, naming the element ("layer 'winter' declares
   <sounds>, which layers cannot yet override") — the 1667 shape, applied here.
2. **Then widen**: each collection gains its identity (region by Id, sound by Id, placement
   by … adjudicate; singletons override whole-or-field, written down) and moves from the
   refusal list to `MergeRows`.
3. **The two merged-but-unproven rows get their proof**: `ByAssetUri` and `ByVehicleName`
   have NO test — test/unit/scenario/ALayerOverridesAnEarlierOneById.cpp exercises kinds
   and instances only. The asset case is the one the parent question names: same Uri, new
   Digest → whole-row replace (Uri is the declared name, Digest the content pin — that
   identity is RIGHT, but it is asserted nowhere).

Proving test extends ALayerOverridesAnEarlierOneById: a layer with a sound refuses by
name; an asset override under the same Uri carries the new Digest; a vehicle override
replaces by Name.

---

Closed: every collection merges or every declaration is spoken for -- nothing is swallowed:

- ROW COLLECTIONS, each by its own identity: providers/generators/compositors by kind;
  regions/doors/volumes/sounds/buses/tables/views by id; events by name; surfaces by
  document; input binds by event; persists by what -- on top of the original four. A
  placement carries no identity, so a layer's placements ADD, on record in the code and the
  trace.
- SINGLETON SECTIONS replace WHOLESALE when the layer declares them (the reader now flags
  Declared on lighting/render/physics/clock; world and drive carried flags already; the
  player by its non-empty Is): the winter mod that dims the light -- the canonical mod --
  replaces <lighting> and the trace says so.
- Every override, add, placement and section replacement lands on the trace.

Proving test: ALayerOverridesAnEarlierOneById -- the winter layer dims 90000 lux to 3000,
re-pins car.glb from digest aaa to bbb by uri (the uri is the name, the digest is the pin),
and overrides a sound by id. 137/137.
