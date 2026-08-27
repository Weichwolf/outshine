Type: feature
State: open
Parent: 1953
Depends: 1897
Supersedes: 1966
Area: sim

# The physics speaks in joints and constraints, and knows no subject

**An engine knows laws and no subjects**, and its whole vocabulary is the one the laws are written
in: body, joint, degree of freedom, drive, constraint, force, contact, integration. **THE PREMISE THIS ITEM WAS FILED ON WAS WRONG, AND THE TRUTH IS BETTER.** It said the tree has no
joints at all. It has one, and it was misnamed: `Physics::Contact` carried `ReachM`,
`StiffnessNPerM`, `DampingNsPerM`, `TravelM`, `StopNPerM` and `LimitN` -- a free length, a spring,
a damper, a travel limit, an end-stop rate and a force limit -- and `Press` computed `k*x` within
the travel plus `k_stop*(x-travel)` beyond it plus `c*xdot`. There is no friction in it, no normal,
no patch and no grip. That is the complete statement of a PRISMATIC JOINT with a spring drive and
an end stop, and nothing else. It is now `Physics::Prismatic` and `Mount::Strut`, and the
behaviour is unchanged: 16 of 16 physics cases green across the rename, which is the whole proof a
rename can offer.

So the shortcut is narrower than filed. The strut is a real joint, correctly modelled. What is
still collapsed is the REST of the wheel: the revolute spin and the revolute steering axis live as
`SteeredShare`, `DrivenShare` and `BrakedShare` -- fractions on a mount rather than degrees of
freedom -- and the contact patch lives as a `Slip` beside them. Both benchmarks keep that assembly
in the GAME layer: RAGE's `CWheel` and Unreal's Chaos vehicles sit above `phConstraint` and
`FConstraintInstance`. Here it is the only thing there is.

`Actuator` has the same shape of error, one level up. **There is no actuator in physics.** There
are constraints with a target and a force limit, which is what Chaos and PhysX call a joint DRIVE
-- position, velocity, force limit -- and a drive is part of the constraint's own statement rather
than an object beside it. board:1897 turned four loose car numbers into a catalogue of two, which
was the right direction and stopped one level short.

What this buys beyond correctness of vocabulary: a door, a crane, a suspension arm, a walker and a
robot are all the same machinery, and none of them is expressible today. A body that is not a
raycast car cannot be declared at all.

- [x] the prismatic joint the tree already had is NAMED one: `Physics::Prismatic`, `Mount::Strut`.
      Proof: 16 of 16 in harness/outshine/physics unchanged, and the two vendor cases green
- [ ] a joint declares two bodies, a kind and its degrees of freedom
- [ ] a drive on a degree of freedom carries a target, a limit and a ratio, and `Actuator` is gone
- [ ] a contact is a constraint the solver reads, not a spring bolted to a point
- [ ] the raycast-and-spring wheel survives as a DECLARED ASSEMBLY in the catalogue, which is where
      RAGE and Unreal keep it, and the drive still drives on it
- [ ] a door and a suspension arm are declarable, proven by a case that opens one
