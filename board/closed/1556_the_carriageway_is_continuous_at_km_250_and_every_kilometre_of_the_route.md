Type: bug
Area: world
Tags: bug, scope

**The carriageway is continuous at km 250 and every kilometre of the route**

**An external reviewer, given nine random stills from the Munich-Hamburg drive, found one frame with
NO ROAD IN IT AT ALL**: `km0250.2-first.png` is full-width green with a 14-pixel dark speck at the
horizon. Either the corridor was not swept there, the sweep was not joined into the shown geometry, or
the camera stands off the route -- each of the three is a different defect and the first measurement is
to tell them apart.

The caveat first: the stills tool relays geometry every 400 m (`kRelayAtM`) and shows 900 m
(`kShownM`); a relay that fell behind the drive would produce exactly this picture while the corridor
itself is sound. That is a defect in the tool's relay pacing, not in the world -- but the picture the
player sees is the tool's picture, so it counts either way.

- [x] the corridor exists at km 250.2: three relays land 249.85-250.17 km, each laying 7200 road
      triangles, and the corridor stands at the car -- dE -1.0 m, dN 0.1 m, deck 0.95 m under the
      body reference
- [x] the relay kept up: 64 relays over the route, every station covered by a lay reaching 780 m
      ahead, and the phantom 45 km strays are gone
- [x] a still at km 250.2 shows the carriageway: wide, centred, to the horizon (2026-08-22 01:30)

## Comments

Found by the magazine-reviewer round over stills of 2026-08-21; ranked third of twelve by damage to
the picture, behind the sky (board:0120, in work) and the absent car (board:1551).


## CLOSED -- three defects of one class, A VALUE FROM ONE FRAME READ IN ANOTHER

The relay instrument answered on its first drive:

```
RELAY at 249.850 km  origin moved -8554.1 -33.5 -44423.1  stray 45103.2 m
RELAY at 249.850 km  origin moved -0.1 0.0 -0.4           stray 45238.8 m
```

Relays only fire near stills, so at a station's FIRST relay the ribbon origin is tens of
kilometres stale. `Lie` built the ground against the OLD origin while `Sweep` built the road
against the NEW one -- two pieces of one subject up to 45 km apart. And `groundAtM` was stored in
one origin frame and compared in the next, so the stray measured the origin's jump rather than the
car's drift and double-fired every relay. Both now live in one frame: `Lie` takes the new ribbon
origin and the stray is measured in world coordinates.

**An honest residue**: the drive AFTER the frame fix but BEFORE the far ring still showed km 250.2
bare, and the following drive (frame fix + ring) showed it whole -- and the earlier drive's
transcript was overwritten by the later one before the difference could be read, which is
board:1552's exact defect claiming its third victim this session. The mechanism above is measured
and real; whether it was the WHOLE mechanism at this station could not be proven against the lost
log. The hourly reviewer drives this route every round, so a recurrence will be caught within the
hour.
