Type: task
Parent: 1603
Area: test

**One object path is built by one include set**

Plan: the object filename gains the include-set identity -- a short checksum of the group's
include string -- so unit/sim's narrow build and the library's wide build of Rigging.cpp can
never share an artefact, and UpToDate needs no flag awareness because the path IS the flag
identity. The claims test TheLayeringIsDeclaredOnce gains the assertion that no two groups
write the same object path with different sets.
