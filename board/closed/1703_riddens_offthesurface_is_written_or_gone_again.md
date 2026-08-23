Type: bug
Area: sim
Regresses: 1613

**Ridden::OffTheSurface is written or gone — again**

1613 closed on "DriveTick.h carries only written fields". The move-2c restructuring
(d1b93a82) reintroduced the class: `Ridden::OffTheSurface` (src/sim/DriveTick.h:48) has no
writer — DriveTick.cpp reads `read.OffTheSurface` (the `Physics::Reading` field, which Rig.cpp:37
does write) at lines 163 and 177 but never assigns `out.OffTheSurface`. A consumer reading the
tick's public product gets a silent zero it can mistake for "never left the surface".

Demanded: `out.OffTheSurface = read.OffTheSurface` where the tick decides it, or the field
leaves `Ridden` (the `OffTheRoad`/`LeftTheRoadAtM` pair already carries the verdict). The
1613 lesson stands: a mirror-field between `Reading` and `Ridden` needs a claims-style check
that every `Ridden` field has a writer, or this regresses a third time.

---

Closed -- the field left Ridden (OffTheRoad/LeftTheRoadAtM already carry the verdict; nothing
in the tree read it), and the 1613 lesson is a CLAIMS GATE now:
EveryTickProductFieldHasAWriter parses Ridden's fields from DriveTick.h and demands a writer
through `out.` in DriveTick.cpp for each -- the third return of this class goes red the day
it lands. Negative control: re-adding the field turns exactly that claims test FAIL.
