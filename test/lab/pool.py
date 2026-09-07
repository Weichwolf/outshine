"""ALL SIX CORES, AND THE ANSWER IS THE SAME ONE EVERY TIME.

Two performance cores and four efficiency ones, and the GIL means a thread pool spends exactly
one of them on work that is arithmetic and shapely. Everything the lab builds is made of pieces
that do not read each other -- a body, a junction, a way, a tile -- so the pieces go to processes.

THREE RULES, AND THEY ARE WHY THIS IS ONE FILE RATHER THAN FIVE COPIES.

    ORDER IS DECLARED, NEVER COMPLETION. `CLAUDE.md`: anything assembled from work that ran on
    more than one worker is combined in the order it was declared in. `Pool.map` gives that;
    `imap_unordered` would render a different picture on a busy machine

    FORK, NOT SPAWN. The children inherit the inputs copy-on-write, so a body, a network or a
    DEM cache is not pickled to get there and only the finished ARRAYS come back. macOS defaults
    to spawn, which would cost more than the pool buys

    AND ONE CORE IS ALWAYS AVAILABLE. `OUTSHINE_NOPOOL=1` runs the same work in this process, so
    "the pool changed the answer" is a comparison rather than an argument. A pool that will not
    start is not a defect either -- it says so and carries on

The shared state travels in a module global set before the fork, because that is what fork is
FOR. Nothing written by a worker is read by another one.
"""
import os

CORES = int(os.environ.get("OUTSHINE_CORES", "0")) or os.cpu_count() or 1
LEAST = 8                          # under this the fork costs more than the work
JOB = None                         # whatever `over` was given, inherited rather than pickled

_WORK = None


def _run(item):
    return _WORK(item)


def over(items, work, shared=None, cores=None, least=LEAST):
    """`work(item)` for every item, in the items' own order. `shared` is put where a forked
    worker can read it as `pool.JOB`."""
    global JOB, _WORK
    items = list(items)
    JOB, _WORK = shared, work
    if len(items) < least or os.environ.get("OUTSHINE_NOPOOL"):
        return [work(i) for i in items]
    want = int(cores or CORES)
    try:
        import multiprocessing as mp
        with mp.get_context("fork").Pool(want) as pool:
            return pool.map(_run, items, chunksize=max(1, len(items) // (want * 8)))
    except Exception as why:
        print(f"    pool refused ({type(why).__name__}: {why}); one core", flush=True)
        return [work(i) for i in items]
