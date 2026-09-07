# A street lies FLAT on a hill, and its footway does not fold

**State:** open · **Lab rank:** 5 · **Waits on:** board:2161 · **Found:** 2026-09-07, the first street-level look at a real place

## What is wrong

Standing in a `living_street` in Rothenburg at 1.7 m, the carriageway in front of the eye is a
CHAOS of tilted plates: a zebra crossing painted on a surface that ramps up like a skateboard
jump, black vertical wedges rising out of the asphalt, and the footway on both sides folding into
steps. From 26 m up the same place reads as a town; the defect is invisible from above because
every plate is near-horizontal in plan and only its HEIGHT is wrong.

It took three instruments to see it at all, and all three are new:

| | |
|---|---|
| `OUTSHINE_EYE=agl,bearing,pitch[,fov[,dx,dy]]` | the client's own camera stands above the roofs, so the whole street pass had never been looked at in a real place |
| the eye stands on the ground UNDER it | a place's origin is a coordinate a surveyor chose; the street 41 m west is 7.27 m lower, so 1.7 m over the origin is a first-floor window |
| a picture NEWER than the script | a failed render leaves the previous one on disk, and three judgements were made on one stale image |

## Where to look first, and what NOT to assume

`kerbline.Surface` answers the height of the drawn carriageway at a point. On the carriageway it
interpolates the containing triangle exactly. OFF it -- a kerb, a footway, a corner fillet -- it
blends the EIGHT NEAREST triangles by inverse square distance, which was put there to stop a
corner fillet jumping between two legs' crowns. On a hill, eight nearest triangles can belong to
ways whose heights differ by metres, and the blend then answers a height that is on none of them.

**That is a hypothesis and not the cause.** The measurement that decides it: the height of every
kerb-ring vertex against the height of the nearest point ON the carriageway, over a real extract,
as a distribution. If the blend is the cause the tail is metres; if it is not, it is centimetres
and the fold is somewhere else -- the ground CDT's `z_edge`, or the road bed's own solve on a
slope, or the walk's 2.5 % fall accumulating over a long ring.

## What the two references do

| | |
|---|---|
| **RAGE** | a road is a spline with a cross-section swept along it; the pavement is part of the same sweep, so it cannot disagree with the carriageway by construction |
| **Unreal** | a spline mesh, the same answer |
| **CARLA** (cited, readable) | `MeshFactory` samples each lane across `vertex_width_resolution` and walls and pavements the RESULT, so the pavement inherits the lane's own elevation profile |

All three sweep. This tree builds the footway as an AREA -- one ring around the whole network --
because that is what makes a junction a place rather than two ribbons crossing (I17, I18). The
area is right; what is missing is that its height must come from the SWEEP and not from a
nearest-neighbour query. **The decision to write down: an area's height is a function of the
NEAREST CARRIAGEWAY EDGE's own station, not of the nearest triangles.**

## The measurement that shows I was wrong

Two numbers over a real extract, both today unmeasured:

- the 99th percentile of |kerb-top height − (carriageway edge height + upstand)| over every ring
  vertex. It must be under 0.02 m; the control is the tree as it stands
- the worst SLOPE of a footway triangle. RASt 06 bounds a footway's longitudinal fall at 6 % and
  its cross fall at 2.5 %, so anything over 10 % is a fold and not a ramp
