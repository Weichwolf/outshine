Type: bug
Parent: 1583
Area: scene
Tags: parity

**Link and Relink share ONE checker — today Relink respells three rules and drops two**

The parity law (1583) demands one checker with identical refusal text from every door. The
relink verb (803d7ae3) violates it inside a single file:

- src/scene/Store.cpp:184-228 (Relink) re-spells the TargetRoles, SameRole and Acyclic
  checks inline — the same strings ("does not reach" 208/238, "joins likes" 211/241, "may
  not close a loop" 218/258) now live twice in the file. A future rule edit has two places
  to miss.
- Relink checks NOTHING of `SourceDoes` (Store.cpp:247) and `Requires` (Store.cpp:250).
  Reachable divergence: `Assigned` requires `Uses` (Register.h:83). Link the mind's tool,
  Link an assignment, Remove the tool — the mind no longer stands on Uses. `Link(mind,
  Assigned, other)` refuses; `Relink(mind, Assigned, other)` succeeds. Two verbs, two
  truths about one rule.
- The proving test says the quiet part: TakingTheWheelIsOneRelink.cpp:52 claims "the same
  checker that guards Link" — no shared checker exists.

Demanded: extract the rule check into one private guard both verbs call (Relink passing the
held-pair exemption for the Exclusive rule), and a test case where the Requires divergence
above refuses through BOTH verbs with the same text.

---

Closed: `Store::Permit(how, from, to, retarget)` is the ONE guard — ends standing, TargetRoles,
SameRole, Exclusive (skipped on retarget: the held pair is the exemption), SourceDoes, Requires,
Acyclic — and both verbs call it; Relink keeps only its verb-identity refusals (non-exclusive
relation, empty seat) and the atomic retarget. The inline respellings at Store.cpp:184-228 are
gone; every rule string stands once. Proving test:
test/unit/scene/TakingTheWheelIsOneRelink.cpp — the Requires divergence (tool removed, Assigned
refused) now refuses through BOTH verbs and the test compares the refusal TEXTS for equality.
127/127 warm.
