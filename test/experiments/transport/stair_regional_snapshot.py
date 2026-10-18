"""Export complete real shared-origin geometry trials for a fresh independent reader."""

import argparse
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from stair_module_experiment import write_json
from stair_patch_inputs import fresh
from stair_regional_positions import encode_regions
from stair_regional_precision import boundary_vertices, position_payload
from stair_terrain_experiment import frozen


def original(path):
    with np.load(path, allow_pickle=False) as data:
        points, faces = data["xy"], data["faces"]
    if points.ndim != 2 or points.shape[1] != 2 or not np.isfinite(points).all():
        raise ValueError("finite original whole terrain XY required")
    anchors = boundary_vertices(faces)
    origin = np.round((points.min(axis=0) + points.max(axis=0)) / 128) * 64
    peers = {
        "legacy": np.zeros(2),
        "west": origin + [-64, 64],
        "east": origin + [64, -64],
    }
    return points, faces, anchors, origin, peers


def run(args):
    commit = frozen()
    fresh(args.output)
    points, faces, anchors, origin, peers = original(args.input)
    args.output.mkdir()
    artifacts, cases = {}, []
    for peer_name, peer_origin in peers.items():
        peer = position_payload(points[anchors], peer_origin)
        for method in ("copy", "rebase_if_exact"):
            streams = encode_regions(
                points, origin, [(anchors, peer, peer_origin)], method=method
            )
            name = peer_name + "-" + method + ".npz"
            np.savez_compressed(
                args.output / name,
                positions=streams[0],
                origins=streams[1],
                origin_ids=streams[2],
            )
            artifacts[name] = digest(args.output / name)
            cases.append(
                {
                    "name": name,
                    "peer": peer_name,
                    "method": method,
                    "origin_count": len(streams[1]),
                    "position_bytes": streams[0].nbytes,
                    "origin_bytes": streams[1].nbytes,
                    "origin_id_bytes": streams[2].nbytes,
                }
            )
    report = {
        "commit": commit,
        "input_sha256": digest(args.input),
        "vertices": len(points),
        "faces": len(faces),
        "boundary_vertices": len(anchors),
        "cases": cases,
        "artifacts": artifacts,
        "scope": "Complete 2D original terrain and actual neighboring Float32 payloads; standalone representation trial, no wider world, height/volume or GPU pipeline admission.",
    }
    write_json(args.output / "comparison.json", report)
    return report


def arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    return parser.parse_args()


if __name__ == "__main__":
    result = run(arguments())
    print({key: result[key] for key in ("vertices", "faces", "boundary_vertices")})
