Type: feature
State: open
Area: world
Tags: scope

**A course in free space goes direct and bends round what is there**

**The owner's ruling, and it is the second half of how anything moves:** *in cities people walk on
footways, cars drive on roads and trains run on rails and cannot leave. In a forest a human, an animal
or a motorbike only avoids OBSTACLES and goes as directly as possible. Flying is different again.*

That is two problems and not one, and the field names them apart:

| | Where the line comes from | What is planned | What is followed |
|---|---|---|---|
| **network** | the graph's own geometry -- footway, carriageway, rail | a sequence of edges | the corridor's reference line |
| **free space** | the straight line to the goal | nothing, until something is in the way | a line deformed round obstacles |
| **airspace** | free in three dimensions, with airways where they exist | a sequence of fixes | a reference line with a real height profile |

**`board:1516` is the layer BELOW all three and is done.** A pilot holds a reference line; where that
line came from is not its business. This item is where the line comes from when there is no network.

## What must be true

- [ ] **A free course is a reference line to the goal**, so a walker in a forest and a car on a road
      are followed by the same pilot with the same numbers
- [ ] **An obstacle deforms it and never replaces it.** The direct line stays the intent; what avoids a
      trunk is a local deviation with a declared clearance, and the deviation ends
- [ ] **The clearance is the body's own**, so a motorbike threads what a truck goes round -- a width and
      a height read from the declaration rather than a per-kind rule
- [ ] **What is in the way comes from the one spatial index**, never a second one built for steering
- [ ] **A course that cannot be found is a REFUSAL and not a straight line through a tree**
- [ ] **Terrain is an obstacle like any other**: a slope past what the mode can climb is in the way, and
      that is how a walker goes round a cliff without anything knowing what a cliff is

## Comments

The published local-steering answers to look at when this is built: Reynolds' steering behaviours, the
dynamic window approach, and velocity obstacles for the case where the thing in the way is moving. The
one thing to take from all three is the MECHANISM -- a deviation computed in velocity space rather than
a path replanned -- because replanning per frame is the cost the corridor exists to avoid.
