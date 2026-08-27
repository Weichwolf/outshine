Type: feature
State: open
Parent: 1581
Area: world
Tags: scope

# A course in free space goes direct and bends round what is there

**Benchmark** — Unreal: navmesh for walkers, splines for anything on rails, and free flight is a controller with avoidance. RAGE: nodes for vehicles, navmesh for peds. **Both agree** — free space is not a graph problem; it is direct plus avoidance of what is there.

In cities people walk on footways, cars drive on roads and trains cannot leave the rails. In a
forest a human, an animal or a motorbike only avoids OBSTACLES and goes as directly as possible.
Flying is different again. The pilot below all three is done — it holds a reference line and
where the line came from is not its business; this is where the line comes from when there is no
network.

| | where the line comes from | what is planned |
|---|---|---|
| network | the graph's own geometry | a sequence of edges |
| free space | the straight line to the goal | nothing, until something is in the way |
| airspace | free in three dimensions, airways where they exist | a sequence of fixes with a height profile |

## What will be true

- [ ] A free course is a reference line to the goal, so a walker in a forest and a car on a road
      are followed by the same pilot with the same numbers.
- [ ] An obstacle DEFORMS it and never replaces it: the direct line stays the intent, the
      deviation has a declared clearance and it ends.
- [ ] The clearance is the body's own — a motorbike threads what a truck goes round — read from
      the declaration rather than a per-kind rule.
- [ ] What is in the way comes from the ONE spatial index, never a second one built for steering.
- [ ] A course that cannot be found is a REFUSAL and not a straight line through a tree.
