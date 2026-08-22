Type: task
Parent: 1621
Area: core

# The span sweep's own residues convert

TileWatermark::Done takes span; the stray Info(span) overload dies now that every wrapper
forwards a view; Log's unit path stops allocating per call -- the unit field joins the write
without building a vector.
