"""Check native rectangle-union variants against an independent cell oracle."""

import argparse
import hashlib
import json
import random
import struct
import subprocess
import tempfile
from pathlib import Path


def oracle(rectangles, side):
    result = bytearray(side * side)
    for left, right, top, bottom in rectangles:
        for y in range(top, bottom + 1):
            for x in range(left, right + 1):
                result[y * side + x] = 1
    return result


def fixtures(side):
    last = side - 1
    middle = side // 2
    points = [
        (x, x, y, y)
        for x in {0, last, min(63, last), min(64, last)}
        for y in {0, last, middle}
    ]
    bars = [(0, last, middle, middle), (middle, middle, 0, last)]
    rng = random.Random(174703 + side)
    sparse = []
    for _ in range(40):
        x, y = rng.randrange(side), rng.randrange(side)
        sparse.append(
            (x, min(last, x + rng.randrange(5)), y, min(last, y + rng.randrange(5)))
        )
    return {
        "points": points,
        "bars": bars,
        "sparse": sparse + sparse[:4] * 10,
        "full": [(0, last, 0, last)],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    output.relative_to(Path(tempfile.gettempdir()).resolve())
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        parser.error("output must be empty")
    rows = []
    for side in (1, 63, 64, 65, 129, 256, 1024):
        for name, rectangles in fixtures(side).items():
            folder = output / f"{side}-{name}"
            folder.mkdir()
            trace = folder / "rectangles.bin"
            trace.write_bytes(b"".join(struct.pack("<4I", *r) for r in rectangles))
            command = [
                "nice",
                "-n",
                "10",
                str(args.binary.resolve()),
                str(trace),
                str(side),
                str(folder),
            ]
            with (folder / "run.log").open("w") as log:
                process = subprocess.run(
                    command, stdout=log, stderr=subprocess.STDOUT, check=False
                )
            row = {
                "side": side,
                "case": name,
                "command": command,
                "returncode": process.returncode,
            }
            rows.append(row)
            if process.returncode != 0:
                raise RuntimeError(f"native fixture failed: {folder}")
            actual = (folder / "mask.bin").read_bytes()
            if actual != oracle(rectangles, side):
                raise ValueError(f"independent union mismatch: {folder}")
            row["mask_sha256"] = hashlib.sha256(actual).hexdigest()
    (output / "result.json").write_text(
        json.dumps({"cases": rows, "success": True}, indent=2) + "\n"
    )


if __name__ == "__main__":
    main()
