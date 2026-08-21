Type: feature
Area: clients
Tags: scope

**The window stands up a WORLD, and the driver sits in it**

**The owner's ruling, and it resets what the windowed driver is:**

> *outshine must be able to draw complex scenes and should already be able to. ECS and compositor must
> handle worlds with thousands of entities and millions of triangles. `tools/driver/` must, in its GUI
> real-time mode, draw the F31 from the driver's perspective, the correct world from the elevation
> data, all OSM roads, infrastructure and buildings, sky, sun, moon and stars -- outshine can do
> real-time day and night cycles. And think of the headlights and the lights on and in the car.*

**`board:1536` was aimed at the wrong door.** It asked for a transform on a draw so that a road and a
car could both appear -- which is true of `Clients::Studio`, the ONE-SUBJECT path a render case uses.
But the engine already has a world path: `src/world/World`, `TerrainLoader`, `OsmField`,
`StreetField`, `BuildingField`, `WaterField`, `VegetationTemplates`, `Data::StarBands`, and the sky
chain the render plan draws -- Transmittance, MultiScatter, SkyView, Irradiance. `src/clients/Sim.cpp`
is 558 lines of exactly this.

**So the windowed driver's job is not to place two subjects. It is to stand up the world the car is
driving through and put the car in it.**

## What must be true

- [ ] **The driver's GUI mode stands up a WORLD**, streamed around the car, and not a single glTF
      subject
- [ ] **The F31 is an entity in that world**, drawn from the driver's seat, with the corridor under
      it and everything else around it
- [ ] **Terrain from the elevation data, roads and infrastructure and buildings from OSM**, all of it
      the same data the drive already reads -- `board:1529` says the road deforms the terrain and
      this is where that becomes visible
- [ ] **Sky, sun, moon and stars, on a real clock**, so a night drive is a night drive
- [ ] **Headlights, tail lights and the cabin's own lights**, which is what makes a night drive
      legible and what `PunctualLight` is already for
- [ ] **Thousands of entities and millions of triangles**, held at 720p60 -- which is the frame budget
      deciding things rather than being quoted

## Comments

**What is already built and measured stays true**: the corridor is swept from the same `StandAt` the
wheels stand on, drawn at 1280x720 with a worst frame of 3.14 ms; the camera comes from the scenario's
declared views in both persons; a player takes the wheel from the mind and gives it back. Those were
the right pieces. What was wrong was believing the road plus the car was the whole picture.

`board:1536`'s transform is still needed -- a car that moves through a static world needs one -- but it
is a detail inside this, not the thing itself.
