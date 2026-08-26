Type: feature
State: open
Parent: 1953
Depends: 1897
Supersedes: 1966
Area: sim

# The physics speaks in joints and constraints, and knows no subject

**An engine knows laws and no subjects**, and its whole vocabulary is the one the laws are written
in: body, joint, degree of freedom, drive, constraint, force, contact, integration. Measured
against that, this tree has no joints AT ALL -- `grep -rl Joint src --include=*.h` finds
`src/content/gltf/`, the skinning joints of a glTF, and nothing in the physics.

What stands instead: `Contact`, a point with a spring and a damper that touches ground. That is a
raycast-and-spring wheel, and it collapses three joints into one shortcut -- a prismatic strut, a
revolute wheel spin, a revolute steering axis. It is a fine MODEL and both benchmarks use it, but
they use it where it belongs: RAGE's `CWheel` and Unreal's Chaos vehicles are the GAME layer above
`phConstraint` and `FConstraintInstance`. Here it is the only thing there is, so the shortcut is
the physics.

`Actuator` has the same shape of error, one level up. **There is no actuator in physics.** There
are constraints with a target and a force limit, which is what Chaos and PhysX call a joint DRIVE
-- position, velocity, force limit -- and a drive is part of the constraint's own statement rather
than an object beside it. board:1897 turned four loose car numbers into a catalogue of two, which
was the right direction and stopped one level short.

What this buys beyond correctness of vocabulary: a door, a crane, a suspension arm, a walker and a
robot are all the same machinery, and none of them is expressible today. A body that is not a
raycast car cannot be declared at all.

- [ ] a joint declares two bodies, a kind and its degrees of freedom
- [ ] a drive on a degree of freedom carries a target, a limit and a ratio, and `Actuator` is gone
- [ ] a contact is a constraint the solver reads, not a spring bolted to a point
- [ ] the raycast-and-spring wheel survives as a DECLARED ASSEMBLY in the catalogue, which is where
      RAGE and Unreal keep it, and the drive still drives on it
- [ ] a door and a suspension arm are declarable, proven by a case that opens one
