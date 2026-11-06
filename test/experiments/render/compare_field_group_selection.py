"""Measure panoramic field candidates and whole-cell admission on cached metadata."""

import argparse
import hashlib
import json
import math
import platform
import struct
import subprocess
import time
from pathlib import Path

import numpy as np

from field_group_selection import FieldGroups, enu_rows
from source_geometry_costs import Cache, ecef, inputs, source_records


def metadata(cache, base):
    basis = hashlib.sha256(("prepared-building-basis-2/" + base).encode()).hexdigest()
    record = cache.db.execute("SELECT * FROM assets WHERE parent=? AND kind=?",
                              (basis, "building-source-metadata")).fetchone()
    if record is None:
        raise ValueError("existing native source metadata required")
    data = cache.load(record)
    if len(data) != 297 or struct.unpack_from("<Q", data)[0] != 0x31524353444C4942:
        raise ValueError("complete building source metadata v1 required")
    return struct.unpack_from("<3d", data, 8), struct.unpack_from("<64I", data, 41)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--database", type=Path, required=True)
    parser.add_argument("--eye", type=float, nargs=3, required=True,
                        metavar=("LAT", "LON", "ASL_M"))
    parser.add_argument("--fov", type=float, required=True)
    parser.add_argument("--viewport", type=int, nargs=2, required=True)
    parser.add_argument("--error-px", type=float, required=True)
    parser.add_argument("--tiles", type=int, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if (not all(map(math.isfinite, (*args.eye, args.fov))) or not 0 < args.fov < 180
            or not -90 <= args.eye[0] <= 90 or not -180 <= args.eye[1] <= 180
            or args.tiles <= 0):
        parser.error("finite geodetic eye, perspective FOV and positive tile count required")
    cache = Cache(args.database)
    cache.db.execute("BEGIN")
    records = list(cache.db.execute(
        "SELECT g.* FROM assets g WHERE g.kind='building-source-groups' AND EXISTS("
        "SELECT 1 FROM assets b JOIN assets m ON m.parent=b.key "
        "WHERE b.parent=g.parent AND b.kind='building-basis' "
        "AND m.kind='building-source-metadata')"))
    available = {record["key"] for record in records}
    records = [record for record in source_records(cache) if record["key"] in available]
    eye = ecef(*args.eye)
    records.sort(key=lambda r: sum(max(r["min" + a] - eye[i], 0,
                                      eye[i] - r["max" + a])**2 for i, a in enumerate("xyz")))
    if len(records) < args.tiles:
        raise ValueError("requested metadata inventory is incomplete")
    began, results = time.perf_counter(), []
    for record in records[:args.tiles]:
        started = time.perf_counter()
        members, sources = inputs(cache, record)
        anchor, counts = metadata(cache, record["parent"])
        actual = np.bincount(members["cell"], minlength=65)[1:]
        if not np.array_equal(actual, counts):
            raise ValueError("source index and current source metadata disagree")
        focal = args.viewport[1] / (2 * math.tan(math.radians(args.fov) / 2))
        planner = FieldGroups(sources.tree, anchor, eye, enu_rows(anchor), focal,
                              args.viewport, args.error_px)
        old, _ = sources.tree.cut(eye, focal, args.error_px)
        new, visited = planner.cut()
        results.append({"base_key": record["parent"], "sources": len(members),
                        "centred": planner.costs(old, members["cell"], verify=True),
                        "panoramic": planner.costs(new, members["cell"], verify=True),
                        "visited_nodes": visited, "seconds": time.perf_counter() - started})
    code = ("compare_field_group_selection.py", "field_group_selection.py",
            "source_geometry_costs.py", "source_geometry_cut.py", "source_product_cut.py")
    report = {"scope": "candidate cost model, not native pose/material/coverage or GPU proof",
              "commit": subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip(),
              "platform": platform.platform(), "numpy": np.__version__,
              "code_sha256": {name: hashlib.sha256(Path(__file__).with_name(name).read_bytes()).hexdigest()
                              for name in code},
              "inputs": vars(args) | {"database": str(args.database), "output": str(args.output)},
              "native_metadata_bytes": cache.native_bytes,
              "stored_package_bytes": cache.package_bytes,
              "seconds": time.perf_counter() - began, "tiles": results}
    report["totals"] = {kind: {name: sum(tile[kind][name] for tile in results)
                               for name in results[0][kind]}
                         for kind in ("centred", "panoramic")}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({key: report[key] for key in ("totals", "seconds", "native_metadata_bytes")}))


if __name__ == "__main__":
    main()
