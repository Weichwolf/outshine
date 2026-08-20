Type: task
Parent: 1498
Area: world
Tags: scope

**A railway is a corridor whose limits are far tighter**

**OSM carries `railway=rail`, `subway`, `tram`, `light_rail` and `narrow_gauge` with exactly the same
bridge and tunnel problem the roads have**, so railways are not a second system -- they are
`board:1499`'s corridor with a different profile and different numbers. **But the numbers are what makes
this the harder case and therefore the better instrument.**

| | road | railway |
|---|---|---|
| maximum gradient | 6 to 12 % | **under 4 %, mainline nearer 1.25 %** |
| minimum radius | tens of metres | **hundreds, and a function of speed** |
| the transition curve | good practice | **compulsory** -- a straight meeting a curve without a clothoid puts a step in the lateral force |
| cross-slope | drainage | **cant**, an engineered quantity balancing centrifugal force at design speed |
| lateral freedom | a driver steers around things | **none at all** |

**So the elevation solve of `board:1500` must know which corridor it is solving.** A railway crossing a
valley cannot ramp down and up the way a road can -- the gradient limit forbids it -- so the
viaduct is *longer* and the inference the gradient limit produces is *stronger*. **A railway is where
the ungagged-structure derivation does most of its work.**

## What must be true

- [ ] **A track is a corridor with a GAUGE**, and the gauge is read from `gauge=*` where OSM says and
      defaulted per country where it does not
- [ ] **Cant is derived from radius and design speed**, not tagged -- so a curve leans by the amount
      that balances the force, which is what makes a train ride and a camera look right
- [ ] **The limits are the railway's** in the elevation solve, so an inferred viaduct on a line is the
      length a railway needs rather than the length a road needs
- [ ] **A transition is always a clothoid**, because a railway that jumps curvature puts a step in the
      lateral force and there is no driver to absorb it
- [ ] **The rails, the sleepers and the ballast are a swept profile** like every other corridor's
      cross-section, and they reduce on the ladder together
- [ ] **A switch is where corridors meet with declared continuity**, which is `board:1499`'s junction
      with one difference: the diverging route's radius is the switch's own and is far tighter

## Comments

**Rails constrain laterally, so a train has ONE degree of freedom and a car has two.** That is what
makes `board:1508` a purer instrument: a car's autopilot can absorb a small defect by steering, and a
train cannot absorb anything. *Every defect a train finds is a defect in the world, with no third
candidate cause.*
