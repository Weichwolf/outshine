Type: issue
Area: tests

# A render-drive member tests a superseded door

TheCarDrivesFromMunichToHamburg plans through the free Path::Plan stub, whose own error text
documents it was never fed ("Network::Weave and Network::Plan are built and nothing feeds them
OsmField's ways"). The real door -- harvest ways, weave the network, plan through it -- is
proven daily by the tools/driver Munich case (39/0). The member was invisible until 1641
relinked the suite; it has failed by design since the stub. Adjudicate: rewrite the member
against the woven-network door at unit scale, or delete it as replaced (the driver case IS the
proof). Until then the drive suite reads 2/3 with this failure named.

---

Adjudicated: DELETED AS REPLACED. The member exercised only the free Path::Plan stub, whose
own error text documented it was never fed; the woven-network door -- harvest, weave, plan --
is proven daily by the tools/driver Munich case (39/0) and the drive suite's
ACarDrivesTheRoadThisEngineBuilt. Rewriting at unit scale would have duplicated that member;
the continental ambition (a route across a streaming graph) is 1503's feature, not this
test's reach. The stub went with its only caller (Wayfinding.h:100 and its body), and the
then-unused Fit.h include followed. Proving state: render/outshine/drive 1/1 PASS, fast gate
127/127 -- the suite no longer carries a failure named by design.
