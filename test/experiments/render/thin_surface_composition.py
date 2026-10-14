"""Compare straight and coverage-weighted optical filtering on two surface models."""
import json
from pathlib import Path
from time import perf_counter

import numpy as np

from outshine_ref.vegetation import LeafOpticsTable


def inputs(size, table):
    y, x = np.indices((size, size))
    coverage = ((x * 7 + y * 13) % 11 >= 4).astype(float)
    sheet = ((x * 3 + y * 5) % 7 < 4).astype(float)
    wood = np.array([0.3, 0.14, 0.05]) * (0.6 + 0.4 * (x % 5) / 4)[..., None]
    pigments = np.stack((15 + 10 * (x % 5), 0.1 + 0.1 * (y % 3)), -1)
    optical = table.sample(pigments.reshape(-1, 2)).reshape(size, size, 6)
    reflection, transmission = optical[..., :3], optical[..., 3:]
    return np.concatenate(
        (
            wood * (1 - sheet)[..., None],
            reflection * sheet[..., None],
            transmission * sheet[..., None],
            sheet[..., None],
            coverage[..., None],
        ),
        -1,
    )


def halve(values):
    n = len(values) // 2
    return values.reshape(n, 2, n, 2, values.shape[-1]).mean((1, 3))


def response(values, cosine):
    return (
        values[..., :3] * max(cosine, 0)
        + values[..., 3:6] * max(cosine, 0)
        + values[..., 6:9] * max(-cosine, 0)
    ) / np.pi


def compare(size, table):
    raw = inputs(size, table)
    weighted = raw.copy()
    weighted[..., :10] *= raw[..., 10:11]
    reference = [response(weighted, cosine) for cosine in (-0.75, 0.25, 0.9)]
    straight = raw.copy()
    maximum_joint, maximum_straight, levels = 0.0, 0.0, 0
    start = perf_counter()
    while len(weighted) > 1:
        weighted, straight = halve(weighted), halve(straight)
        reference = [halve(value) for value in reference]
        cover = weighted[..., 10:11]
        resolved = np.divide(
            weighted[..., :10], cover, out=np.zeros_like(weighted[..., :10]), where=cover > 0
        )
        for cosine, expected in zip((-0.75, 0.25, 0.9), reference):
            maximum_joint = max(
                maximum_joint, float(np.max(abs(response(resolved, cosine) * cover - expected)))
            )
            maximum_straight = max(
                maximum_straight,
                float(np.max(abs(response(straight, cosine) * straight[..., 10:11] - expected))),
            )
        energy = np.max(resolved[..., 3:6], -1) + np.max(resolved[..., 6:9], -1)
        assert np.all(energy <= resolved[..., 9] + 1e-12)
        levels += 1
    assert maximum_joint < 1e-12
    return dict(
        pixels=size,
        levels=levels,
        joint_error=maximum_joint,
        straight_error=maximum_straight,
        elapsed_ms=1000 * (perf_counter() - start),
    )


def main():
    source = Path('lib/src/outshine_ref/vegetation/data/leaf-optics-table-1.json')
    table = LeafOpticsTable.from_recipe(json.loads(source.read_text()))
    rows = [compare(size, table) for size in (64, 128, 256)]
    out = Path('build/experiments/render/thin-surface')
    out.mkdir(parents=True, exist_ok=True)
    (out / 'composition.json').write_text(json.dumps(rows, indent=2) + '\n')
    for row in rows:
        print(json.dumps(row))


if __name__ == '__main__':
    main()
