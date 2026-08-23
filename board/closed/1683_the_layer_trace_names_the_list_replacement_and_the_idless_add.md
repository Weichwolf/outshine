Type: bug
Area: scenario
Tags: trace, 1493 follow-up

# The layer trace names the list replacement and the id-less add

1493's first box: what overrode what is PUBLISHABLE — "a declaration nobody can trace is a
declaration nobody can debug". Two events the merge now performs are not spoken:

- **A redeclared output/stage list replaces silently.** 1679's semantic (ScenarioRead.cpp:188-189
  clears on `Count("output"/"stage") > 0`) destroys the base's list, but the only trace a
  render-declaring layer earns is ScenarioLayer.cpp:170-171 "merged into the render --
  omitted attributes keep the base's values" — true for attributes, silent about the two
  collections that were just wholesale replaced. The vehicle's replace speaks
  (ScenarioLayer.cpp:145 "replaced the vehicle"); the stage list must too.
- **An id-less row traces as nothing.** ScenarioLayer.cpp:26-27 prints
  `same.Identity(row)`; for ByInstanceId/ByIdField on an id-less row that is the empty
  string — "layer 'mod' added instance ''". The trace should name what it can (the
  instance's `Of`, the volume's `Fires`…) or say "an id-less <what>".

Proof: extend ALayerOverridesAnEarlierOneById — a stage-redeclaring layer's trace contains
"replaced", an id-less instance's trace names its kind.

---

Closed: the stage/output list replacements SPEAK on the trace ("replaced the stage list --
the declared list is the list"), and an id-less row traces as "(id-less)" instead of a pair
of naked quotes. 1493's publishable box holds everywhere the merge acts.
