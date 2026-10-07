import argparse
import hashlib
import json
import re
from pathlib import Path


def model(path, views, unbound_maps):
    payload = path.read_bytes()
    log = payload.decode()

    def metric(name):
        found = re.search(re.escape(name) + r"\s+(\d+\.\d+)", log)
        if not found:
            raise ValueError(f"{path}: missing {name}")
        return float(found.group(1))

    prototypes = int(metric("cost.assets.prototypes.hits"))
    fallback_uploads = prototypes * views * unbound_maps
    return {
        "log": str(path),
        "log_sha256": hashlib.sha256(payload).hexdigest(),
        "prototype_hits": prototypes,
        "views_per_prototype": views,
        "unbound_maps_per_view": unbound_maps,
        "fallback_uploads_per_draw_before": fallback_uploads,
        "shared_configurations_upper_bound": unbound_maps,
        "avoided_uploads_per_draw_lower_bound": max(0, fallback_uploads - unbound_maps),
        "measured_rebuild_staging_allocations": metric("rebuild: staging buffer allocation attempts"),
        "gpu_allocation_bytes": None,
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("logs", type=Path, nargs="+")
    parser.add_argument("--views", type=int, required=True)
    parser.add_argument("--unbound-maps", type=int, required=True)
    args = parser.parse_args()
    if args.views < 1 or args.unbound_maps < 1:
        parser.error("views and unbound map count must be positive")
    print(json.dumps([model(path, args.views, args.unbound_maps) for path in args.logs], indent=2))
