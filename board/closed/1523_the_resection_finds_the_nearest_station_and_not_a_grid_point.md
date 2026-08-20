Type: bug
Area: generators
Tags: instrument bug

**The resection finds the nearest station and not a grid point**

`ReferenceLine::Nearest()` scanned the window at 1 m and then "refined" with fixed steps of 0.1, 0.01,
0.001 and 0.0001 -- **0.1111 m in total, against a coarse interval of 1 m.** It could not cross half
a grid cell, so the station it returned was pinned near whichever metre mark the coarse scan happened
to pick, and it JUMPED by a whole metre when the coarse minimum stepped.

**A 1 m station jump on a 4 % grade is a 0.04 m step in the surface height under every contact**, which
is 1280 N per contact, 3.18 m/s2 across the car, and at a 1 ms step a jerk of 3180 m/s3.

Now it brackets the coarse minimum by one coarse step either side and runs a golden section over it --
40 iterations, so the interval closes to about 1e-8 m.

Measured on the drive suite's negative control, the same run before and after:

| | before | after |
|---|---|---|
| worst jerk at the profile's speed | 1133.09569 m/s3 at 20.9089 m | 4.38540658 m/s3 at 835.013592 m |
| worst jerk held to 25 m/s | 1133.09569 m/s3 at 20.9089 m | 1.6862753 m/s3 at 34.5741287 m |
| worst deviation held to 25 m/s | 0.100036743 m | 0.0990219923 m |

**A factor of 258 in the jerk, and every bit of it was the instrument.**

Proven by `test/render/outshine/drive/ACarDrivesTheRoadThisEngineBuilt.cpp`, whose jerk floor is now
checked to stay under 5 m/s3 on a road with no defect in it.

## Comments

**What caught it was publishing the STATION beside the number, and nothing else would have.** 1133
m/s3 is a plausible-looking jerk. What is not plausible is the same value at 20.9089 m in every run
at every speed -- a road does not do that. The check that reads *worst jerk* had been green under a
threshold I chose; the note that reads *where that happened* was what made it false.

**This is the negative control earning its cost on its first run.** Without it, every one of those
spikes on real OSM data would have been read as a step in the road, classified, ranked, and chased.
See `board:1518`: a finding whose cause is the instrument costs a whole round, and the only defence is
a road that is known to have nothing wrong with it.

**And it was mine, written the same day.** The fixed-step descent looked like refinement and was a
search that could not reach.
