Type: issue
Area: actor
Tags: correctness, pathfinding

**A turn refusal is a property of an edge pair, and the search walks edge states**

`Network::Plan` (src/actor/path/Wayfinding.cpp:275-326) is A* over NODES with node settling
(`settled[node]`, line 278-279), but the turn-radius refusal at lines 293-314 depends on the
PREDECESSOR (`came[node]`, `cameLengthM[node]`). That constraint makes the state space the
directed edge, not the node — the textbook turn-restriction result — and node settling makes
the search wrong in both directions:

- a node settled via a predecessor whose onward turn is too tight refuses edges that a
  DIFFERENT approach would take legally; the node never re-opens, so a legal route reads as
  "network in pieces" or detours — `TurnsRefused` counts phantom refusals;
- conversely `came[node]` is whatever the last relaxation wrote, so the turn is judged
  against an arrival the final path may not even use.

The repair is decided by the sources the tree already cites (edge-based Dijkstra is how every
shipped router carries turn costs, and 1521's .ynd checklist stores per-LINK data for exactly
this reason): the open/settled sets key on (edge in), or equivalently each node splits per
incoming edge. The proof is a fixture where the short approach forbids the turn and the long
approach allows it — node-based search refuses, edge-based search finds the long way round.
