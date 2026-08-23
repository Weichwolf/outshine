Type: bug
Area: gltf
Tags: boundary, bounded-terms

# The gltf reader's cost is bounded like its shapes

Rounds 20/21 taught the reader to refuse hostile SHAPES (1726 cycles, 1727
sizes, 1728 container). Its COSTS are still the file's to command, at 27caf0ca:

- The forest proof walks every node to its root with no memo
  (src/gltf/Document.cpp:806-815): a parent CHAIN of n nodes costs n²/2 steps.
  n is bounded only by the JSON itself — a few-MB file of chained nodes buys
  minutes of CPU before the (correct) verdict. Mark-visited or path-halving
  makes the same proof O(n).
- A viewless accessor's zero-fill is capped at the CONTAINER ceiling
  (src/gltf/Document.cpp:1521-1524): count · components · sizeof(double) may
  reach ~4.3 GiB — an out.assign of half this device's RAM from a 200-byte
  file, and the comment beside it claims the cap protects. The fill is backed
  by NO bytes; its bound must come from what the file actually carries (e.g.
  sparse present and sparse data in-bounds), not from uint32.
- ResolveBuffers slurps the ENTIRE external uri file before comparing to the
  declared length (src/gltf/Document.cpp:434-440): a uri naming a 100 GB file
  is read to the end, then refused. Read at most declared+1 bytes and refuse
  on shortfall or surplus.

Demanded: each of the three costs bounded by what the file legitimately
declares, with a refusal proof per arm in
test/unit/gltf/AFileThatCannotMeanAnythingIsRefusedByName.cpp, and a timing
sanity (the chain fixture completes in linear time).
