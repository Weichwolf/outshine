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

- [ ] the corridor exists at km 250.2 (dump the sweep's vertex count over km 249-251)
- [ ] the relay kept up (log relay distance behind the camera over the whole drive, publish the max)
- [ ] a still at km 250.2 shows the carriageway after the fix

## Comments

Found by the magazine-reviewer round over stills of 2026-08-21; ranked third of twelve by damage to
the picture, behind the sky (board:0120, in work) and the absent car (board:1551).
