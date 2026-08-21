Type: bug
Area: world
Tags: bug

**Sampling the ground mid-drive is SAFE**

`fb_stream_ground` crashes in `Data::SourceSet::Ask` when it is called repeatedly at stations along the
route, and does not crash when it is called once. The faulting chain, from the macOS crash report:

```
Data::SourceSet::Ask(Request const&) const
World::TilePool::FetchInto(Request const&, Landing*)
World::TilePool::BytesBlocking(Request const&, Landing*)
(anonymous)::OracleTerrain::Take(int, uint32_t, uint32_t)
World::TerrainTiles::RawGrid / StitchedGrid
(anonymous)::GroundTileAt(long, long)
```

**This survives the ownership fix.** `Journey::Lay` used to build the `ContentStore` and `SourceSet` on
its own stack while `fb_stream_open` kept a reference to them; they are owning members of
`Journey::State` now and one sample after `Lay` works. **Many samples still fault**, so something else
in the streamer's state does not outlive what it is asked to do.

**`BytesBlocking` is the shape to look at**: a blocking fetch, on the caller's thread, from a pool
whose workers were started for the laying phase.

## What must be true

- [ ] **The ground can be asked for a height at any time after the world opens**, as often as a caller
      likes -- it is the field the physics samples for every contact, so it is already asked millions
      of times during a drive
- [ ] **`fb_stream_open`'s global goes**, which `board:1527` filed and which is the reason a lifetime
      is unanswerable by reading one function
- [ ] **A refusal, not a fault**, if the streamer cannot answer

## Comments

**Found while publishing cut and fill per station**, which the goal requires -- *cut and fill are
published per metre of road, because a road that fills 30 m is a viaduct nobody marked*. One station
is measured today (0.41 m above raw ground at km 17.3) and that is all this crash allows.

**And the number matters beyond the accounting**: in the driver's view the ground now fills the frame
and **the carriageway is not in it**. Where the road sits BELOW raw terrain it is buried by the ground
the renderer draws, which is the same defect seen from the other side. Cut and fill is not a report --
it is what makes the road visible.
