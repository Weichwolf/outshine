Type: issue
Area: clients
Tags: perf

**The previous pose is positions, not the whole subject**

`Live::Pose` (`src/clients/Live.cpp:227`) runs `Previous_ = Geometry_` on every animated frame:
a deep copy of the entire `Gltf::Subject` -- positions, normals, tangents, uvs, colours,
indices, parts -- at 60 Hz, on the frame path, inside the block `TookPosing_` instruments.

Every consumer reads two things and nothing else: `Previous->PositionsM()` and `VertexCount()`
(`src/clients/GltfStudio.cpp:260-263,357,390` -- the velocity stream and its size guard). The
other attribute arrays are copied each frame so that nobody looks at them.

What must be true: the previous pose is a positions buffer, double-buffered by swap -- the copy
that remains is the one the velocity stream actually reads, and `TookPosing_` says so. One
animated subject pays megabytes of memcpy per frame today; board:1538's city multiplies it.
