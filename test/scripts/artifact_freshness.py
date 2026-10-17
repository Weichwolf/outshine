"""Compare produced artifact timestamps without losing subsecond dependency changes."""

from pathlib import Path
import sys


def fresh(output, inputs):
    try:
        timestamp = Path(output).stat().st_mtime_ns
        return all(Path(path).stat().st_mtime_ns <= timestamp for path in inputs)
    except OSError:
        return False


if __name__ == '__main__':
    raise SystemExit(0 if fresh(sys.argv[1], sys.argv[2:]) else 1)
