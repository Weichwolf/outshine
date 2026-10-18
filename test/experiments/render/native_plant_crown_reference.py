"""Compare demanded native crown groups with unchanged independent Python poses."""

import argparse
import hashlib
import json
import subprocess
import sys
from pathlib import Path
from unittest.mock import patch

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "vegetation"))
from crown_shoots import CrownShoots
from outshine_ref.vegetation import Shoot


def cases(growth):
    points = np.array([[0, 0, 0], [0, 0, 1], [0, 0, 2], [1, 0, 2], [0, 0, 3]], float)
    parents = np.array([-1, 0, 1, 2, 2], np.int32)
    for depth in (0, 1, 5):
        for count in (0, 1, 5, 64, 1000):
            yield f"fork-{depth}-{count}", points, parents, count, 17, depth, True
    deep = np.zeros((70, 3))
    deep[:, 2] = np.arange(70)
    yield (
        "deep-high-seed",
        deep,
        np.arange(-1, 69, dtype=np.int32),
        1000,
        2**64 - 1,
        1.1,
        True,
    )
    for seed in (682, 718):
        for limbs in (0, 1):
            source = np.load(growth / f"oak-{seed}-limbs-{limbs}.python.npz")
            yield (
                f"oak-{seed}-{limbs}",
                source["positions"],
                source["parents"],
                90000,
                seed,
                1.1,
                seed == 682 and limbs == 1,
            )


def queries(reference, whole):
    hosts = reference.hosts
    selected = list(map(int, hosts[::-1] if whole else hosts[[0, len(hosts) // 2, -1]]))
    result = [(node, 100000) for node in selected]
    result.extend([(selected[0], 100000), (0, 100000), (2**32 - 1, 100000)])
    if len(reference.variants):
        node = int(hosts[0])
        result.append((node, max(0, int(reference.counts[0]) - 1)))
    return result


def encode(reference, requests):
    lines = [
        f"{len(reference.parents)} {len(reference.variants)} {len(requests)}",
        (
            f"{reference.seed} {reference.length_m} {reference.bend_rad} "
            f"{reference.petiole_m} {reference.foliage_depth_m}"
        ),
    ]
    for parent, point in zip(reference.parents, reference.positions, strict=True):
        lines.append(f"{parent} " + " ".join(format(value, ".17g") for value in point))
    lines.append(" ".join(map(str, reference.variants)))
    lines.extend(f"{node} {maximum}" for node, maximum in requests)
    return "\n".join(lines) + "\n"


def parse(text):
    summary, counts, costs = None, None, None
    hosts, leaf_counts, groups, errors = [], [], [], []
    identities, poses = [], []
    for line in text.splitlines():
        row = line.split()
        if row[0] == "S":
            summary = list(map(int, row[1:]))
        elif row[0] == "V":
            counts = list(map(int, row[1:]))
        elif row[0] == "H":
            hosts.append(int(row[1]))
            leaf_counts.append(int(row[2]))
        elif row[0] == "G":
            groups.append((int(row[1]), int(row[2])))
        elif row[0] == "E":
            errors.append(int(row[1]))
        elif row[0] == "L":
            identities.append(list(map(int, row[1:4])))
            poses.append(list(map(float, row[4:])))
        elif row[0] == "T":
            costs = list(map(float, row[1:]))
        else:
            raise AssertionError(f"unknown consumer record {row[0]}")
    return (
        summary,
        counts,
        hosts,
        leaf_counts,
        groups,
        errors,
        np.array(identities).reshape((-1, 3)),
        np.array(poses).reshape((-1, 16)),
        costs,
    )


def compare(binary, case, output, roundtrip=False):
    name, points, parents, count, seed, depth, whole = case
    variants = (
        np.arange(count, dtype=np.uint32) % (64 if name.startswith("fork") else 8)
    ).astype(np.uint8)
    with patch.object(
        Shoot, "detail", side_effect=AssertionError("premature leaf poses")
    ):
        reference = CrownShoots(points, parents, variants, seed, foliage_depth_m=depth)
        expected_summary = reference.summary()
    requests = queries(reference, whole)
    text = encode(reference, requests)
    result = subprocess.run(
        [str(binary), *(["--roundtrip"] if roundtrip else [])],
        input=text,
        text=True,
        capture_output=True,
        check=True,
        timeout=30,
    )
    summary, counts, hosts, leaf_counts, groups, errors, ids, poses, costs = parse(
        result.stdout
    )
    assert summary[:4] == [
        len(parents),
        len(reference.tips),
        len(reference.hosts),
        count,
    ]
    assert summary[4] == (0 if roundtrip else len(parents) * 12)
    assert summary[5] == (len(hosts) if roundtrip else len(parents)) * 4
    np.testing.assert_array_equal(hosts, reference.hosts)
    np.testing.assert_array_equal(leaf_counts, reference.counts)
    np.testing.assert_array_equal(counts, np.bincount(variants, minlength=64))
    expected_groups, expected_errors, expected_ids, expected_poses = [], [], [], []
    for node, maximum in requests:
        try:
            group = reference.group(node, maximum)
        except ValueError:
            group = None
        if group is None:
            expected_errors.append(node)
            continue
        expected_groups.append((node, len(group["ids"])))
        expected_ids.append(
            np.c_[group["ids"], group["owner_nodes"], group["variants"]]
        )
        expected_poses.append(
            np.c_[group["origins"], group["nodes"], group["frames"].reshape((-1, 9))]
        )
    expected_ids = np.concatenate(expected_ids)
    expected_poses = np.concatenate(expected_poses)
    assert groups == expected_groups and errors == expected_errors
    np.testing.assert_array_equal(ids, expected_ids)
    np.testing.assert_allclose(poses[:, :6], expected_poses[:, :6], rtol=0, atol=1e-9)
    np.testing.assert_allclose(
        poses[:, 6:15], expected_poses[:, 6:15], rtol=0, atol=1e-12
    )
    assert np.all((poses[:, 15] >= 0.05) & (poses[:, 15] <= 0.95))
    np.savez_compressed(
        output / f"{name}.npz",
        positions=points,
        parents=parents,
        variants=variants,
        native_ids=ids,
        native_poses=poses,
        python_ids=expected_ids,
        python_poses=expected_poses,
    )
    return {
        "case": name,
        "source_nodes": len(parents),
        "source_leaves": count,
        "source_hosts": len(hosts),
        "source_tips": expected_summary["terminal_nodes"],
        "requests": len(requests),
        "refused": len(errors),
        "generated_rows": len(ids),
        "index_build_bytes": summary[4],
        "index_resident_bytes": summary[5],
        "index_ms": costs[0],
        "pose_ms": costs[1],
        "maximum_live_group_bytes": int(costs[3]),
        "position_max_error_m": float(
            np.max(abs(poses[:, :6] - expected_poses[:, :6]), initial=0)
        ),
        "frame_max_error": float(
            np.max(abs(poses[:, 6:15] - expected_poses[:, 6:15]), initial=0)
        ),
        "input_sha256": hashlib.sha256(text.encode()).hexdigest(),
        "native_output_sha256": hashlib.sha256(result.stdout.encode()).hexdigest(),
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", required=True, type=Path)
    parser.add_argument("--growth-cohort", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--roundtrip", action="store_true")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    receipt = {
        "source_head": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "binary_sha256": hashlib.sha256(args.binary.read_bytes()).hexdigest(),
        "numpy": np.__version__,
        "position_atol_m": 1e-9,
        "frame_atol": 1e-12,
        "scope": "finite demanded pose comparison, not surface/radiance/LOD/world acceptance",
        "stored_roundtrip": args.roundtrip,
        "cases": [],
    }
    for case in cases(args.growth_cohort):
        row = compare(args.binary.resolve(), case, args.output, args.roundtrip)
        receipt["cases"].append(row)
        (args.output / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
        print(json.dumps(row), flush=True)


if __name__ == "__main__":
    main()
