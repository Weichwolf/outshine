Type: task
Parent: 1862
Area: apps
Tags: driver, review, product

# One command drives the product and leaves ten stills along the route

The hourly architect judges the PRODUCT, and must not know how it is built. Before this it was
handed a build recipe -- compiler flags, a library path, a corpus directory -- which is an
implementation detail leaking into a reviewer's brief, and which goes stale the day any of the
three moves.

```sh
test/run.sh --drive --from 48.1371,11.5754 --to 48.1583,11.5033
```

builds the library, compiles and links the driver, drives the declared route and leaves **ten
stills, evenly spaced along it**, printing the directory. Everything the command needs is
derived inside `run.sh` from the declarations already there.

Ten and not one, because a defect that appears at one kilometre and not another is only visible
as such against its neighbours -- a single frame cannot tell a systematic fault from a local one.

## What will be true

- [x] `test/run.sh --drive` exists, takes the driver's own options after it, and prints where it
      wrote.
- [x] `--into DIR` on the driver writes N stills evenly along the drive, N = 10 by default.
- [x] The review agent's brief carries the command and nothing about the build.
- [ ] The spacing is by DISTANCE and not by frame. Today `Engine::Advance` does not move along
      a route at all (board:1862), so the ten are evenly spaced over FRAMES, which is the same
      thing only while nothing moves. When the door drives what it accepts, this becomes
      station-spaced and the item's own proof is that two consecutive stills differ.
- [ ] Proving test: the gate's own `--drive`, and a case that asserts ten files appear.
      Negative control: N set to 0 -> none appear and the run says so.

## Comments

- 2026-08-25 -- filed on the owner's instruction mid-round: *"der agent darf keine
  implementations details kennen"* and *"eine fahrt von A nach B sollte immer automatisch 10
  screenshots ablegen in gleichen streckenabständen"*.
