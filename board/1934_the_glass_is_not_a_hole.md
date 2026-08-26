Type: bug
State: open
Parent: 1573
Area: render
Tags: measured, picture, driver, material

# The car's glass carries a tint, a reflection and the cabin behind it

Chase view, `shots-river-chase/along07.png` at bb9472db, the car filling 230 x 190 px:

| where | sRGB |
|---|---|
| through the rear window, y 470..495, x 600..690 | **(64.0, 78.2, 58.1)** |
| the ground beside the car, same rows, x 300..400 | **(64.0, 79.0, 59.5)** |

**1.4 counts apart.** The rear window transmits the background unchanged: no tint, no Fresnel
term at any angle, no interior geometry between the glass and the world behind it. From directly
astern you look through a car and out the far side onto grass.

The same still gives the body's response to the key. Declared: `<key lux="40000"
elevationDeg="42" bearingDeg="150"/>`.

| where | sRGB | what a photograph gives |
|---|---|---|
| roof panel, horizontal, full sun | (16, 30, 54) | the brightest opaque thing in frame |
| tailgate, facing away from the sun | (17, 32, 60) | in shade, dark |
| the grass beside the car | (64, 79, 60) | albedo ~0.2 |

The roof and the tailgate read the SAME to one count -- a horizontal panel in full sun and a
vertical panel in its own shade are indistinguishable -- and both read a quarter of the grass
next to them. Paint of albedo ~0.6 under a 42 deg sun belongs about 3 times ABOVE that grass,
so the subject's diffuse response is roughly 3.5 stops adrift and carries no directional term
at all. board:1932 is the other half of this: the car is inside the dark region there.

board:1569's white blob is still on the roof of every chase still -- the shark-fin antenna reads
as a pale unlit teardrop brighter than the paint around it.

## What will be true

- [ ] The transmissive draw tints and reflects: the luminance behind glass differs from the
      luminance beside it, and the difference varies with incidence.
- [ ] The cabin is behind the glass -- seats and headrests occlude the ground through the rear
      window.
- [ ] A horizontal panel in sun and a vertical panel in shade differ by the cosine the sun's
      elevation gives, measured on the still.
