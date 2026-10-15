"""Same-source exact geographic lookup comparison; not a botanical coverage claim."""

import argparse
import hashlib
import json
import resource
import subprocess
import time
from pathlib import Path

import numpy as np
import shapely
from species_geo_index import CellRegions, DirectRegions, read_regions


def run(source, output):
    started = time.perf_counter()
    codes, geometries, normalizations = read_regions(
        source, union_multipart=True, wrap_longitude=True
    )
    read_s = time.perf_counter() - started
    started = time.perf_counter()
    shapely.prepare(geometries)
    direct = DirectRegions(geometries)
    direct_build_s = time.perf_counter() - started
    started = time.perf_counter()
    index = CellRegions(direct)
    index_build_s = time.perf_counter() - started
    rng = np.random.default_rng(17)
    latitude = np.rad2deg(np.arcsin(rng.uniform(-1, 1, 20000)))
    longitude = rng.uniform(-180, 180, 20000)
    # All exact source vertices exercise boundaries, not just easy interiors.
    vertices = shapely.get_coordinates(geometries)
    latitude, longitude = (
        np.r_[latitude, vertices[:, 1]],
        np.r_[longitude, vertices[:, 0]],
    )
    before = direct.query(latitude, longitude)
    after = index.query(latitude, longitude)
    np.testing.assert_array_equal(before, after)
    cases = (
        ("Swiss Plateau", 47.4, 8.5, "SWI"),
        ("Koerbersee", 47.256121, 10.115857, "AUT"),
        ("DarmstadtWest", 49.875871, 8.662596, "GER"),
        ("Malcesine", 45.744855, 10.800445, "ITA"),
        ("California", 37.77, -122.42, "CAL"),
        ("Tasmania", -42.88, 147.32, "TAS"),
        ("Victoria", -37.81, 144.96, "VIC"),
        ("Kenya", -1.28, 36.82, "KEN"),
    )
    controls = []
    for name, lat, lon, expected in cases:
        matches = index.query(lat, lon)
        result = [codes[i] for i in matches[1]]
        if expected not in result:
            raise ValueError(f"known botanical country mismatch: {name}: {result}")
        controls.append(
            {"name": name, "latitude": lat, "longitude": lon, "codes": result}
        )
    timings = {"direct": [], "cell": []}
    for repeat in range(6):
        sequence = (("direct", direct), ("cell", index))
        for name, implementation in sequence if repeat % 2 == 0 else sequence[::-1]:
            started = time.perf_counter()
            answer = implementation.query(latitude, longitude)
            timings[name].append(time.perf_counter() - started)
            np.testing.assert_array_equal(answer, before)
    # Persist only explicit experiment arrays, not a new public runtime ABI.
    output.mkdir(parents=True, exist_ok=True)
    np.savez_compressed(
        output / "geographic-index.npz",
        table=index.table,
        offsets=index.offsets,
        candidates=index.candidates,
    )
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_modified": bool(
            subprocess.check_output(["git", "status", "--porcelain"], text=True)
        ),
        "source": str(source),
        "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
        "regions": len(codes),
        "normalizations": normalizations,
        "read_s": read_s,
        "direct_build_s": direct_build_s,
        "cell_build_s": index_build_s,
        "index": index.summary(),
        "queries": len(latitude),
        "query_coordinate_sha256": hashlib.sha256(
            np.column_stack((latitude, longitude)).tobytes()
        ).hexdigest(),
        "matched_pairs": before.shape[1],
        "timings_s": timings,
        "peak_process_rss_MiB": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        / 1024,
        "controls": controls,
        "scope": "Same normalized research source, all vertices and spherical-uniform random coordinates. Exact source memberships, not species/ecology/visual/native admission; no source redistribution approval.",
    }
    (output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "regions": len(codes),
                "queries": len(latitude),
                "cell_array_bytes": report["index"]["table_bytes"]
                + report["index"]["candidate_bytes"],
                "median_query_s": {k: float(np.median(v)) for k, v in timings.items()},
            }
        ),
        flush=True,
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    arguments = parser.parse_args()
    run(arguments.source, arguments.output)
