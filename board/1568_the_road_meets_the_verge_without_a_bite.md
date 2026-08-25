Type: bug
State: active
Area: corridor
Tags: bug

**The road meets the verge without a bite**

Round four, three stations: jagged notches where the road ribbon meets the graded verge
(`km0017.3-third` right verge, `km0267.1-first` stepped right edge, `km0666.2-third` a hard
polygonal wedge against the sky). The grading pulls ground posts to the formation inside `keptM`,
but the POSTS are a 3 m grid while the ribbon's edge is a 2 m swept polyline -- two samplings of
one edge, and the moire between them is the bite.

Same family as board:1557 (one width, one edge at a segment join); this is the continuous version
of the same statement: **the ribbon's edge polyline IS the grading's boundary**, not a grid's
nearest approximation of it.

- [ ] the verge mesh snaps its boundary row onto the ribbon's edge polyline, or a skirt closes the
      gap
- [ ] the three stations above show no bite, looked at

## Comments

Filed from the reviewer's fourth round, ranked second of seven.

Round eight gave the class an address and an alibi: the sawtooth is a dozen navy tears down the
right edge at km 17.3 -- the worst instance ever captured, by the new framed camera -- while the
same camera at km 36.5 shows CLEAN edges. Local, not systemic: whatever differs at that station
(a width transition, a junction harvest, a grading hint reset at the route's first relay) is the
mechanism.

Round nine widens this from a station to the CLASS, with the reviewer's own bet on record: the
navy hairline along the road's right edge is systemic (km 17.3, 117.4, 342.3, 708.1-framed, and
as "hood trim" in first person), and "items 1, 4, 7 -- I'd bet they're one bug". The mechanism
that fits all five frames: the ribbon's 0.35 m VERTICAL flank, sun-averted on a south-lit
northbound route, carries only skylight and reads as an inked line wherever it faces the camera
edge-on -- and the tears are the grading grid's 3 m posts sawing across that same flank. A real
carriageway has no visible vertical edge; the shoulder falls away as a bank. The fix candidates:
sweep the outer edge as the 1:1.5 bank it is (the Section already declares the slope family), or
snap the verge boundary onto the edge polyline -- either kills the line and the teeth together.

The kerb is gone: the blend zone beside the shoulder started at FORMATION height (road minus the
0.35 m thickness), so the first visible ground beside every metre of carriageway sat a kerb's
drop below the deck -- the sun-averted face of that step was the systemic ink line, and the 3 m
posts sawing across it were the teeth. The blend starts at DECK height now, and km 17.3's framed
edge shows the teeth collapsed to a soft moire and the ink to a faint residue. What remains is
the 3 m grid against the 2 m edge polyline -- this item's original statement, now the whole of it.

Round ten rules on the bet: LOST, and the ruling is the finding. The kerb fix deleted the hairline
from every clean stretch -- "the cleanest road edges I've seen in ten rounds" at 250.2 and 666.2 --
so the sub-deck blend WAS the systemic cause. But km 17.3's seven tears stand untouched with navy
open INSIDE them (a real gap between deck and verge, not a shading of the step), and the
first-person "hood trim" is the car asset's own silhouette shading, no verge's business. Three
symptoms shared a colour, not a cause. This item's remainder is now precisely km 17.3's local
seam: the station where the verge mesh fails to reach the deck edge at all.

- 2026-08-25, hourly review, the seam is still there and it is still km 17 --
  `$TMPDIR/outshine-stills/km0016.8-framed.png` from the 07:07 drive shows the deck/verge
  boundary as a SAWTOOTH from roughly x=600 to x=1050 in the 1280x720 frame: a run of straight
  steps, each a few pixels of verge biting into the deck and each about the length the 3 m grid
  would give against the 2 m edge polyline. This is the item's own remaining statement, measured
  again two rounds later and unmoved. Nothing in the commit delta since touched the verge.
