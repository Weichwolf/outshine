Type: task
Parent: 1382
Area: gltf
Tags: khronos

**Gltf 2 0 base is audited against the reader once by enumeration**

**A grep proves a string absent, never a capability.** The reader's own refusals name one base gap, which
says what it REFUSES and nothing about what it silently ignores. This task walks the glTF 2.0
specification's property tables and states, per property, whether the reader reads it, refuses it or
drops it.

- [ ] The enumeration is the specification's, not the corpus's
- [ ] Every DROPPED property becomes its own work item, or the audit produced a document instead of work
- [ ] The audit is a test where it can be -- a property the reader must not drop is better held by a
  case than by a table
