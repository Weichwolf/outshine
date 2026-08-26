Type: feature
State: active
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
      proof: harness/outshine/physics/ScoreWhatATaskTreeDoes
- [ ] the existing pilot stands as a leaf under a tree without changing what it computes
- [ ] a driver that must yield abandons the subtree and resumes, proven by a case over a route
