"""Independent original-vertex validation and opt-in prepared-support comparison."""

import json
import time

import numpy as np
from beam_intervals import exact, sub
from prepared_source_support import affine_ranges, load_product
from prepared_support_visibility import select_sources


def check_vertices(product, source):
    """Find an enclosed local witness for every original used vertex; never fit padding."""
    arrays = product["arrays"]
    checked, unavailable, required = 0, [], 0.0
    for ordinal in np.unique(source["binding"][:, 1]):
        at = int(np.searchsorted(arrays["sources"]["ordinal"], ordinal))
        if (
            at >= len(arrays["sources"])
            or arrays["sources"]["ordinal"][at] != ordinal
            or arrays["state"][at] != 1
        ):
            unavailable.append(int(ordinal))
            continue
        faces = source["indices"][source["binding"][:, 1] == ordinal]
        points = source["positions"][np.unique(faces)].astype(float)
        frame, bounds = arrays["frames"][at], arrays["local_bounds"][at]
        local = (points - frame[0]) @ np.linalg.inv(frame[1:])
        witness = np.clip(local, bounds[0], bounds[1])
        if not np.isfinite(witness).all():
            raise ValueError("nonfinite local support witness")
        distance = sub(exact(points), affine_ranges(exact(witness), frame))
        upper = float(np.maximum(abs(distance[0]), abs(distance[1])).max())
        if upper > arrays["padding"][at]:
            raise ValueError(f"prepared support misses original source {ordinal}")
        required = max(required, upper)
        checked += len(points)
    return {
        "checked_used_vertices": checked,
        "unavailable_sources": unavailable,
        "max_world_component_witness_distance_m": required,
        "scope": "original cell only; witnesses check enclosure, never change it",
    }


def apply_prepared(path, input_path, document, source, projection, output):
    manifest = json.loads((input_path.parent / "manifest.json").read_text())
    identity = {
        "parent": document["parent"],
        "archive_sha256": manifest["original-inputs.tar.gz"]["sha256"],
    }
    product = load_product(path, identity)
    projection["support_identity"] = identity
    projection["anchor_ecef"] = document["anchor_ecef"]
    containment = check_vertices(product, source)
    start = time.perf_counter()
    hidden, available, work = select_sources(product, projection["sources"], projection)
    seconds = time.perf_counter() - start
    if np.any(hidden & ~projection["hidden"]):
        raise ValueError("prepared outer support contradicts original point hull")
    control_hidden = int(projection["hidden"].sum())
    projection["hidden"] = hidden
    request = {
        k: np.asarray(projection[k]).tolist()
        for k in ("basis", "observer_box", "anchor_ecef")
    }
    request.update(
        support_identity=identity,
        requested=projection["sources"].tolist(),
        plane=projection["plane"],
        guard_projected_m=projection["guard_projected_m"],
    )
    (output / "prepared-request.json").write_text(json.dumps(request) + "\n")
    np.save(output / "prepared-hidden.npy", hidden)
    np.save(output / "prepared-available.npy", available)
    return {
        "header": product["header"],
        "containment": containment,
        "selection_s": seconds,
        "work": work,
        "original_hull_hidden_control": control_hidden,
        "prepared_hidden_sources": int(hidden.sum()),
        "scope": "prepared target support only; original opaque blocker construction still required",
    }
