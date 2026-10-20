import hashlib
import json
import tarfile
from pathlib import Path

import numpy as np
from native_visibility_cost import classify, groups, load, project

GROUP = np.dtype(
    {
        "names": ["low", "high", "info"],
        "formats": [("<f4", 4), ("<f4", 4), ("<u4", 4)],
        "offsets": [0, 16, 32],
        "itemsize": 48,
    }
)


def digest(path):
    value = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            value.update(block)
    return value.hexdigest()


def extract(archive, output):
    metadata = json.loads((archive.parent / "manifest.json").read_text())
    if metadata["schema"] != 1 or digest(archive) != metadata["archive_sha256"]:
        raise ValueError("visibility archive differs from its manifest")
    expected = metadata["canonical"]
    output.mkdir()
    seen = set()
    with tarfile.open(archive, "r:xz") as packed:
        for member in packed:
            if (
                not member.isfile()
                or member.name not in expected
                or Path(member.name).name != member.name
                or member.name in seen
                or member.size != expected[member.name]["bytes"]
            ):
                raise ValueError("unexpected visibility archive member")
            target = output / member.name
            with packed.extractfile(member) as source, target.open("wb") as destination:
                for block in iter(lambda: source.read(1 << 20), b""):
                    destination.write(block)
            if digest(target) != expected[member.name]["sha256"]:
                raise ValueError("unpacked visibility input differs from its manifest")
            seen.add(member.name)
    if seen != set(expected):
        raise ValueError("visibility archive is incomplete")
    return metadata


def partition(snapshot, rows, order, keys):
    selected = rows[order].copy()
    _, starts, counts = np.unique(keys[order], return_index=True, return_counts=True)
    if not np.array_equal(starts, np.r_[0, np.cumsum(counts)[:-1]]):
        raise ValueError("batch groups are not contiguous")
    jobs = np.zeros((len(rows), 4), dtype="<u4")
    jobs[:, 1] = np.repeat(np.arange(len(starts), dtype=np.uint32), counts)
    jobs[:, 2] = selected["info"][:, 2]
    jobs[:, 3] = selected["info"][:, 1] * 3
    for first, count in zip(starts, counts):
        models = snapshot.models[selected["info"][first : first + count, 0]]
        if not (models == models[0]).all():
            raise ValueError("position-only batch contains different transforms")
    maximum = np.add.reduceat(jobs[:, 3].astype(np.uint64), starts)
    args = np.zeros((len(starts), 5), dtype="<u4")
    args[:, 1] = 1
    args[:, 2] = np.r_[0, np.cumsum(maximum)[:-1]]
    args[:, 4] = selected["info"][starts, 0]
    baseline_args = args.copy()
    baseline_args[:, 0] = maximum
    indices = np.concatenate(
        [snapshot.indices[int(job[2]) : int(job[2]) + int(job[3])] for job in jobs]
    ).astype("<u4")
    batches = np.column_stack((starts, counts)).astype("<u4")
    return selected, jobs, batches, args, baseline_args, indices


def model_blocks(snapshot, rows, model_ids):
    order = np.argsort(model_ids[rows["info"][:, 0]], kind="stable")
    sorted_ids = model_ids[rows["info"][order, 0]]
    changes = np.flatnonzero(np.r_[True, sorted_ids[1:] != sorted_ids[:-1]])
    keys = np.empty(len(rows), dtype=np.uint32)
    batch = 0
    for begin, end in zip(changes, np.r_[changes[1:], len(rows)]):
        for first in range(begin, end, 256):
            keys[order[first : min(first + 256, end)]] = batch
            batch += 1
    return order, keys


def prepare(source, output):
    snapshot = load(source)
    original_args = np.fromfile(source / "arguments.bin", dtype="<u4").reshape(-1, 5)
    selected_args = original_args[snapshot.jobs[:, 1]]
    if (selected_args[:, 1] != 1).any() or (selected_args[:, 3] != 0).any():
        raise ValueError("this replay requires single instances and zero base vertices")
    output.mkdir()
    snapshot.models.astype("<f4").transpose(0, 2, 1).copy().tofile(
        output / "models.bin"
    )
    np.fromfile(source / "pyramid.bin", dtype="<f4").tofile(output / "pyramid.bin")
    view = np.fromfile(source / "uniform.bin", dtype="<u4")
    np.r_[view.view("<f4")[32:48], view.view("<f4")[24:28]].astype("<f4").tofile(
        output / "raster-view.bin"
    )
    _, model_ids = np.unique(
        snapshot.models.reshape(-1, 16), return_inverse=True, axis=0
    )
    manifest = []
    total = int(snapshot.jobs[:, 3].astype(np.uint64).sum())
    for limit in (0, 64, 32, 16, 8):
        low, high, models, weights = groups(snapshot, limit)
        rejected, _, _ = classify(
            snapshot, project(snapshot, low, high, models), weights, 4
        )
        parents, firsts = [], []
        for parent, (_, _, first, count) in enumerate(snapshot.jobs):
            cuts = np.arange(0, int(count), limit * 3 if limit else int(count))
            parents.extend([parent] * len(cuts))
            firsts.extend(int(first) + cuts)
        rows = np.zeros(len(weights), dtype=GROUP)
        rows["low"][:, :3], rows["high"][:, :3] = low, high
        rows["info"][:, 0] = parents
        rows["info"][:, 1] = weights
        rows["info"][:, 2] = firsts
        expected = (weights * (~rejected)).astype("<u4")
        uniform = view.copy()
        uniform[28], uniform[60] = len(weights), 1
        uniform.tofile(output / f"view-{limit}.bin")
        for policy in ("source", "block"):
            order, keys = (
                (np.arange(len(rows)), snapshot.jobs[rows["info"][:, 0], 1])
                if policy == "source"
                else model_blocks(snapshot, rows, model_ids)
            )
            values = partition(snapshot, rows, order, keys)
            if len(values[-1]) != total:
                raise ValueError("regrouping changed the original triangle population")
            label = f"{policy}-{limit}"
            for name, value in zip(
                (
                    "groups",
                    "jobs",
                    "batches",
                    "args",
                    "baseline-args",
                    "baseline-index",
                ),
                values,
            ):
                value.tofile(output / f"{label}-{name}.bin")
            expected[order].tofile(output / f"{label}-expected.bin")
            manifest.append(
                {
                    "label": label,
                    "limit": limit,
                    "policy": policy,
                    "groups": len(rows),
                    "batches": len(values[2]),
                    "indices": total,
                }
            )
    (output / "pipeline-manifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n"
    )
    return manifest
