Type: bug
Parent: 1767
Area: sim
Tags: two-truths, measured, drive

# The lane the pilot aims in is the lane the room is measured in

Found by attributing `board:1767`'s wheel-off-the-carriageway at km 113.990. The two numbers
that decide whether a wheel is on the road are read at **different stations**.

`src/sim/CorridorLay.cpp:310-314` bakes the fine arrays from a coarse post:

```cpp
for (size_t fine = 0; fine < fineAside.size(); ++fine) {
  const size_t post = (size_t)((double)fine * fineM / spanM);
  fineAside[fine] = asideM[band];
  fineEdge[fine] = halfWidthM[band];
}
```

`src/sim/DriveTick.cpp:66` reads the aim at the FINE index and `:156, :199` read the lane at the
COARSE one:

```cpp
const size_t fine = (size_t)(at.AlongM / fineM);
const double wantAsideM = fineAside[fine];              // aim: baked at floor(fine*fineM/spanM)
...
const size_t post = (size_t)(at.AlongM / spanM);
laneHalfM[post]                                          // room: read at floor(AlongM/spanM)
```

`fine * fineM <= at.AlongM`, so the baked band is `floor(fine*fineM/spanM)` and the read post is
`floor(at.AlongM/spanM)`. **They differ by one whenever a station boundary falls inside a fine
step** -- which is every post boundary, because `fineM < spanM`.

## Measured at the crossing

```
NOTE the lane it was in                = 3.750000 m      -> its centre is -1.875 m
NOTE what the pilot was aiming for     = -2.125000 m     -> that is a 4.25 m lane's centre
NOTE how far the aim still had to travel = 0 m           -> the aim was NOT mid-move
```

`asideM[post] = -0.5 * (lanes - 1) * laneM` with `laneHalfM[post] = 0.5 * laneM` are written in
ONE loop (`CorridorLay.cpp:270-282`) from one `halfWidthM[post]` and one `lanes`, so they cannot
disagree **at the same post**. The aim says 4.25 m and the room says 3.75 m, and the rate
limiter had finished, so the two were read one station apart.

The consequence is exactly `board:1767`'s failure: the pilot holds its car at the centre of a
lane that is 0.25 m wider than the one the room is measured in, and 0.25 m is a quarter of the
0.97 m the lane leaves either side of a 1.811 m car.

## What will be true

- [ ] The aim, the corridor edge and the lane half-width are read at ONE station -- one array
      family at one resolution, or all three baked fine from the same band.
- [ ] Proving test: a synthetic corridor whose lane width steps, driven across the step, in
      which the aim and the room agree at every station. Negative control: the two indices put
      back -- the aim belongs to one lane and the room to its neighbour, and the test names the
      station.
- [ ] `board:1767` is re-measured against the repair: the deviation at km 113.990 was 0.888 m
      of a 0.970 m margin, and 0.25 m of it is this.
