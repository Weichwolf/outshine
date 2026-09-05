Type: feature
State: open
Area: render, scenario, generators
Tags: look, camera, declaration

# The default world is graded, and a look is DECLARED

**Benchmark** -- Unreal puts the whole chain in POST-PROCESS VOLUMES an author places and blends by
priority: an ACES tonemapper with local exposure, colour grading with a LUT, film grain, motion
blur driven by a shutter angle, and a `CineCameraActor` carrying a real sensor size, focal length
and aperture, so depth of field is computed rather than dialled. RAGE puts it in the TIMECYCLE:
every visual parameter keyframed against hour of day and weather and interpolated, with a named
MODIFIER overriding it over a region. Taken: RAGE's STRUCTURE with Unreal's CONTENTS, because this
engine has no author to place a volume and time, sun and weather are already PROVIDERS -- a look
that is not a function of them would be a second source for the same fact -- while RAGE's keyframes
are hand-drawn curves where this tree can afford the physical camera.

A frame that leaves this engine has been through a camera, an exposure and a grade before anybody
sees it. Not as an option a scenario switches on: the DEFAULT, because a raw linear buffer with a
Reinhard curve is what makes a picture read as a game rather than as a photograph, and this engine's
whole claim is that its picture can be argued with.

## The two halves, and only one of them is taste

**The engineering half is already this tree's law** and is not what this item is about: a road built
to RAS-Q, a building to its epoch's rule, the sun where the clock puts it. That makes a place
CREDIBLE. It does not make it READ.

**The look half decomposes into six carriers, every one of them a number:**

| carrier | what it is | why a picture needs it |
|---|---|---|
| lens | sensor, focal length, aperture; vignette (cos^4 and mechanical), distortion, lateral chromatic aberration, a bokeh with the diaphragm's blade count | a pinhole is the one camera nobody has ever filmed with |
| exposure | a film response with a toe and a SHOULDER, metered from the scene | the highlight rolloff is the single largest carrier of "film" |
| grade | lift/gamma/gain plus a 3D LUT, per look | a committed palette is what a viewer remembers of a place |
| shutter | 180 degrees, so motion blur follows the frame time rather than a slider | 60 fps with no shutter reads as video, never as film |
| grain | signal-dependent, over the graded image | additive noise reads as a broken sensor |
| air | haze, shafts, and the depth they make legible | this is the one carrier the atmosphere already owns here |

## What Unreal does, what RAGE does

**Unreal** puts the whole chain in POST-PROCESS VOLUMES that an author places in the level and
blends by priority: ACES tonemapper with local exposure, colour grading with a LUT, film grain,
motion blur driven by a shutter angle, and a `CineCameraActor` carrying a real sensor size, focal
length and aperture so depth of field is computed rather than dialled. The parameters are PHYSICAL
and the chain is the reference.

**RAGE** puts it in the TIMECYCLE: every visual parameter is keyframed against hour of day and
weather and interpolated between them, and a named MODIFIER pushes an override over a region -- an
interior, a tunnel, a mission. The look is therefore a FUNCTION OF TIME AND WEATHER, and a place
under it changes because the world changed rather than because somebody placed a box.

**Taken: RAGE's structure with Unreal's contents, and the reason is this tree's own architecture.**
outshine has no author to place a volume -- time, sun and weather are PROVIDERS and already answer,
so a look that is not a function of them would be a second source for the same fact. But RAGE's
keyframes are hand-drawn curves, and this tree can afford the physical version: a real sensor, a
metered exposure, an ACES-class curve. So a `Look` is a DECLARED section -- palette, LUT, key bias,
haze, shutter, sensor, aperture -- evaluated against the hour and the weather the providers give,
overridable per region and per scenario, and the ENGINE'S OWN DEFAULT stands when none is declared.
Four directors are then four declared looks over one Earth, which is exactly what a scenario is for.

## The generator's half: MASS SIMPLE, SKIN DEEP

The look is not only post. A baroque church is a box with a dome -- the silhouette stays legible --
and its whole effect is a deeply layered surface in raking light. A hull covered in greebles is the
same grammar, which is why baroque works for science fiction and why both read on a small screen.
So the rule the building generator owes is measurable and not a mood: RELIEF DEPTH per facade area,
and SHADOW-CASTING EDGES per storey. A pilaster projects an eighth of its width, a cornice oversails
0.3 to 0.6 m, a reveal is 0.1 to 0.25 m deep. The LOD ladder in `test/lab/roads/CASES.md` already has
the rungs and stands one step too low: the generator draws the epoch's element set ABOVE its median,
never at it. A median building is what makes a generated street read as generated.

## What measurement shows I was wrong

- **`make shots` at one place, one hour, two looks.** If the declared look and the default produce
  the same digest, the section is not reaching the frame and this item is a no-op
- **The shutter.** Frame time is measured; if motion blur does not follow it when the frame time
  moves, the blur is a slider wearing a physical name
- **Relief.** A count of shadow-casting edges per storey against the epoch's table, on the lab's
  own sheets, and an image beside it. A number that rises while the picture flattens means the
  count is measuring the wrong thing
- **The negative control:** a look with an identity grade, no grain, no shutter and a pinhole must
  reproduce today's digests exactly. If it does not, the chain is not a chain but a rewrite

## Cited

Ridley Scott trained as a set designer and his rule is the one this item can act on: ONE strong
backlight through haze, and a frame packed with FUNCTIONAL detail -- a corridor's conduit, a hull's
plating, a street's signage -- so that depth is read from occlusion rather than from a gradient.
That is the same rule as "mass simple, skin deep", stated for a whole scene instead of a facade.
Nolan is its complement and the bound on it: nothing faked, practical light, real optics, a cold
palette and mass in place of ornament -- which is what this engine already promises when it puts
the sun where the clock says.

`Blade Runner`, `Alien` and `2049` for volumetrics as the primary medium, `Playtime` for a modernist city
built as a set, `Metropolis` and `Caligari` for the expressionist silhouette, `The Shining` for
symmetry and impossible interior geometry, `Dune` for brutalist mass and negative space, Nolan for
the rule this engine already holds -- nothing faked, practical light, real optics. None of these
decides a structure; each names a look that has to be REACHABLE by a declaration.

Waits on: 2129 (reflections), 2140 (clouds), 2128 (many lights). Beside: 2138.
