Type: feature
State: open
Area: data
Tags: scope

**The road network carries what dense traffic needs, and every field says where it came from**

**Looked up rather than recalled**, at the owner's suggestion: GTA V runs one of the densest traffic
networks ever shipped, and its path data is `.ynd` -- a tiled graph of nodes and links. The format is
readable in CodeWalker's `YndFile.cs`, and it is worth taking as a **CHECKLIST**: these are the fields
a shipped engine found it needed in order to drive a city.

**The assumption that comes with it, and it is the whole difference: every one of those fields is
AUTHORED BY HAND.** A person decided this node is a junction, that link has two lanes forward, this
one is a slip road. We derive all of it from OSM or we infer it. So the schema transfers and not one
value does.

## What `.ynd` carries, and where ours has to come from

| `.ynd` field | Where GTA gets it | Where we get it |
|---|---|---|
| `LaneCountForward` / `LaneCountBackward`, 3 bits each, **on the LINK** | authored | `lanes:forward` / `lanes:backward`, else `lanes` split by `oneway`, else the class default |
| `LinkLength`, one byte, precomputed | authored | derived once from the corridor, and STORED -- a route query cannot afford a sqrt per edge |
| `OffsetValue` 3 bits + `NegativeOffset` | authored | our cross-section: the lane centre is an offset from the reference line |
| `IsJunction` | authored | ways sharing a node, or `board:1518`'s un-noded crossing |
| `Tunnel` | authored | `tunnel=*`, else the gradient reveal, else the crossing reveal |
| `Highway` | authored | `highway=motorway|trunk` |
| `SlipRoad`, `IndicateKeepLeft/Right` | authored | `highway=*_link`, and the turn the geometry makes |
| `NoBigVehicles`, `NarrowRoad` | authored | `maxheight` / `maxweight` / `width`, else the class |
| `OffRoad` | authored | `surface=*`, `highway=track` |
| `Density`, 4 bits | authored | **must be DERIVED**, and we have no answer yet |
| `DeadEndness`, 3 bits | authored | derivable: distance in links to the nearest cycle |
| `HeuristicValue`, 7 bits per node | authored | precomputable, and `board:1503` needs exactly this |
| `NoGps`, `DontUseForNavigation`, `Shortcut` | authored | a routing refusal we must state ourselves |
| `AreaID` + `NodeID`, links reference the pair | tiled by construction | our tile id and index -- **a link across a tile boundary is a VALUE, never a pointer** |

## The three transferable facts, taken as mechanism

- [ ] **The lane count belongs to the LINK and not to the node.** A stretch has lanes; a point does
      not. That is also how OSM says it, and how our corridor already says it
- [ ] **The vertical is quantised eight times finer than the horizontal.** `.ynd` stores X and Y at
      1/4 unit and Z at 1/32 -- roughly 0.25 m against 0.031 m. **A shipped engine spent its bits
      where a vehicle feels them**, and our elevation solve should carry the same asymmetry rather
      than treating all three axes alike
- [ ] **The graph is tiled on a fixed grid and links cross by identifier.** GTA SA's ancestor of this
      format used 750x750 unit squares in row-major order from the south-west corner; V keeps the
      shape. A route that leaves the loaded set is the normal case, not an error

## What must be true

- [ ] Every field above is either read from a named OSM tag or DERIVED by a named inference, and
      which of the two is published per field and per region
- [ ] A link stores its length, and the route search never recomputes it
- [ ] Density is derived rather than declared, or it is honestly named as the one field we cannot
      produce -- **a traffic density nobody can derive is a scenario's to declare**
- [ ] Dead-endness is derived, because spawning traffic into a cul-de-sac is a defect a player sees
      in the first minute

## Comments

**Density is the interesting gap.** GTA authored it because a person knows Vinewood Boulevard is
busier than a service alley. From OSM we have `highway=*` class, lane count, and whether the way is
inside a `landuse=residential|commercial|industrial` -- which is a proxy and not a measurement. It may
be that density is genuinely a scenario's declaration and not the world's, and saying so is a better
answer than a plausible formula nobody can check.

Sources: CodeWalker's `CodeWalker.Core/GameFiles/FileTypes/YndFile.cs` for the field and flag layout,
and GTAMods for the tiling that GTA SA introduced and V inherited.
