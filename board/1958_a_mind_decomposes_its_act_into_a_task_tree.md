Type: feature
State: open
Parent: 1953
Area: actor

# A mind decomposes its act into a task tree

**RAGE wins this row over Unreal and it is the clearest win on the board.** A `CTask` owns
sub-tasks, runs until it yields, and is abandoned as a whole SUBTREE when the situation changes:
`drive to X` owns `follow the corridor` owns `keep the lane`, and each level knows only its own
concern. Unreal's Behavior Tree re-decides from the root every tick and carries its state in a
blackboard beside the tree rather than in the tree.

For a PHYSICAL actor the hierarchy is the act's own shape. A driver overtaking is not in a
different leaf of one flat selector -- it is a sub-task that owns the steering for its duration and
hands control back when it finishes or is abandoned.

CURRENT has one pilot that computes a steering angle from a corridor. That is the LEAF, standing
alone with no tree above it, and everything a mind should decide is either absent or hard-coded in
the leaf.

- [x] a task owns sub-tasks, yields, and can be abandoned as a subtree
      proof: outshine/physics/ScoreWhatATaskTreeDoes
- [x] the existing pilot stands as a leaf under a tree without changing what it computes:
      `Control::HoldsLane` wraps `Pilot::Hold` and `Sim::DriveTick` runs it through `Step`. The
      drive reads 10.5115 / 522.756 / 5.31713, the same digits as before the tree existed.
      Negative control: the leaf computing nothing takes the drive 10.5115 m to 1.27116 m.
- [x] the tree LIVES across ticks, which is what yielding and resuming needs: `DriveState` owns
      the lane task, `DriveTick` steps the one it holds, and the drive reports 40 ticks over 40
      frames rather than 40 tasks built and thrown away. The trajectory is unchanged.
- [ ] a driver that must YIELD abandons the subtree and resumes. The machinery stands and the
      SITUATION does not: nothing in a corridor-following drive asks it to give way. Inventing one
      would be building a feature under a refactor that forbids it (board:1953), so this waits for
      a scenario that declares traffic, a junction or an obstacle -- something with a reason to
      yield rather than a switch that says to.
