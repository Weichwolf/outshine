Type: bug
Area: corpus
Tags: khronos

**The preparer and the runner hold two closed sets over one declaration, and they disagree on eight of twenty-six manifests — **Band 1****

`test/corpus/prep/manifest.py:495` — `_fields("manifest.scene.material", value, ("source", "kind"),
("note",))` — is a **closed** field set that does not know `carriedBy`. `test/render/Parity.cpp:433`
**reads** `material["carriedBy"]` and refuses by name when it is wrong (`:254`). Eight manifests carry
the key. So `python3 test/corpus/prepare.py dry-run` refuses **8 of 26** on
`manifest.scene.material.carriedBy`, while the runner requires it.

**Pre-existing, verified by stashing** — unrelated to the `acceptanceClass` key added at `8f0ecce`.

This is § I.20's duplicated-`INC_*` shape in a third place, and the third time this tree has written one
fact twice and had nothing fail when the two drifted: **what a manifest may contain is the schema's, and
the runner restates it by reading keys the schema has never heard of.** A closed set is the right
mechanism and two of them is the defect — the preparer's refuses what the runner needs, and a key the
preparer accepts but the runner never reads would fail in neither.

**Right:** one declaration of the manifest schema that both sides read, so a key exists once. **Fixed
when** `dry-run` accepts every manifest the runner accepts and refuses every one it does not, checked by
running both over the whole corpus — which is a test, not an inspection. **Band 1**: the Khronos work
adds manifests, and every one added under a split schema is added twice.
