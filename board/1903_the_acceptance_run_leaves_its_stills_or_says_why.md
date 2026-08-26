Type: bug
State: active
Parent: 1865
Area: apps
Tags: driver, acceptance, measured

# The acceptance run leaves its stills, or says out loud why it did not

**TRUE at a73c6ca5, verified by the architect in its own worktree.** The one command the map
prints, into a directory that did not exist:

    build/outshine-driver --headless --into /tmp/.../shots --assets .../apps-driver-f31
    DROVE 15466 frames over 2.896 of 2.916 km, kept 10 still(s)

Ten of ten, no `mkdir`, and the four rounds that opened with one are over. `Engine::Capture`
makes the path it was given; the still trigger samples the MIDPOINTS of the ten intervals
(`2 * alongM * N >= (2k+1) * routeM`) rather than their right edges, so the last still no longer
demands 100 % of a route that ends at 99.3 %.

Proving case `test/harness/outshine/door/ScoreWhereAFrameLands.cpp`: a frame captured into a path
three levels deep that no test has made -- kept, the directory stands, the first eight bytes are
PNG's signature. Negative control: `create_directories` removed, REFUSED, no directory, no png.
