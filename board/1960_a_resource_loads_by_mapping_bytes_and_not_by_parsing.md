Type: feature
State: active
Parent: 1953
Depends: 1955
Area: content

# A resource loads by mapping bytes and not by parsing

**RAGE wins this row clearly.** A resource is stored in the layout it will be USED in: the loader
maps the bytes, fixes the pointers, and the object is live -- no parse, no per-item allocation, no
construction pass. Unreal's DDC and cooked assets get part of the way and still parse.

This is not a performance nicety, it is what makes cell streaming affordable: a cell that must
parse its content cannot arrive at the speed a camera moves, and no amount of threading fixes a
per-item cost that scales with content.

The content store is already hash-addressed, so the addressing half is done. What is missing is
that what the hash names is a BLOB in final layout rather than a document to be read.

- [ ] a stored part is in final layout and loading it performs no per-item work
- [ ] loading a part allocates once and parses nothing, proven by a case counting both
- [ ] the glTF reader becomes a BAKER that writes the blob, and the frame path never sees glTF
