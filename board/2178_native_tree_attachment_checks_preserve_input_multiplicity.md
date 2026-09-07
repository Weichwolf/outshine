Type: bug
State: active
Area: tests, generators
Tags: measured, flora
Depends: nothing

# Native tree attachment checks preserve input multiplicity

2123's full physical foliage exposes a mis-specified regression: the native birch test
requires every leaf origin to be globally unique. The growth input itself contains
147853 leaf points but only 147850 unique positions. Foliage selects 45775 leaves
(0.309598 per point), native origins have 45774 distinct positions. The assertion fails
on existing input multiplicity, not on an adapter-generated fan.

Correct contract: retain the multiset of declared attachment positions under the native
metre transform. Coincident input positions remain legal; the adapter may neither merge
other positions into fans nor invent/remove origins. Compare input/output multisets;
restore the grouping of 16 leaves at one origin as a real negative control.
Evidence: build/tree-leaf-attachment-diagnosis.log and its native-tree case log.

This is a correction of a demonstrated wrong specification, not a lowered quality
threshold. No arbitrary tolerated duplicate fraction. Growth morphology and spatial
coverage remain in 2111/2123/2176. No Unreal/RAGE-specific implementation is involved:
this is the ordinary transform/multiplicity contract at a mesh adapter boundary.
