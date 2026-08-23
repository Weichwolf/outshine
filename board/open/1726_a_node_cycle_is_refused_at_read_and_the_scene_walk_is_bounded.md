Type: bug
Area: gltf
Tags: boundary, security, hang

**A node cycle is refused at read and the scene walk is bounded**

The forest check refuses only SHARED children, never cycles:

- src/gltf/Document.cpp:743-753 — `Parent_` is filled and a child with two parents is
  refused. A two-node cycle (`node 0 children:[1]`, `node 1 children:[0]`) gives each node
  exactly ONE parent and passes; so does a self-child (`node 0 children:[0]`). The glTF 2.0
  spec states the node hierarchy MUST be acyclic, and scene nodes MUST be root nodes —
  neither is enforced (a scene may name a node that has a parent, Document.cpp:755-768).
- src/gltf/Subject.cpp:575-586 — the flatten's `pending` walk pushes children with no
  visited set and no step bound. A scene rooted inside a meshless cycle loops FOREVER:
  `Chain()` never runs (its `steps > Nodes_.size()` bound at Document.cpp:1542-1543 only
  fires for nodes that carry a mesh or light). A crafted file at the one content surface is
  a process hang, not a refusal.

Demanded: Document refuses at read — after the parent pass, walk each node to its root with
a step counter bounded by `Nodes_.size()` (or count reachable-from-parents), and refuse a
scene root that has a parent. The unit twin gets the two-node cycle, the self-child and the
parented scene root, all refused by name; the old reader hangs on the first of them.
