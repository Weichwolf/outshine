Type: task
Parent: 1621
Area: core

# The span sweep's own residues convert

TileWatermark::Done takes span; the stray Info(span) overload dies now that every wrapper
forwards a view; Log's unit path stops allocating per call -- the unit field joins the write
without building a vector.


---

Closed: TileWatermark::Done takes span; the Log wrappers are uniform overload pairs
(initializer_list and span per level); the unit rides LogSink::Write as its own parameter, so
the unit path builds NOTHING per call -- the vector and its string copies are gone at the
signature. Fast gate 122/122.