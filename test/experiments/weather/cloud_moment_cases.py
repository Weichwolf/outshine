"""Fixed-partition optical moments, with independent Decimal cell integrals."""

from decimal import Decimal, localcontext

import numpy as np
from cloud_field import Layer, wind

CELLS = 32
QUERY_VECTORS = 6 + CELLS
ANSWER_VECTORS = 1 + CELLS


def query(layer=None, seconds=0.0, origin=(0, 1772, 0)):
    layer = Layer(cover=0.0) if layer is None else layer
    value = np.zeros((QUERY_VECTORS, 4), dtype="<f4")
    value[0] = [layer.radius_m, layer.base_m, layer.thickness_m, layer.extinction_per_m]
    value[1, 0] = layer.cover
    value[2, :3] = wind(layer) * seconds
    value[3] = [*origin, 1e8]
    value[4] = [0, 1, 0, 1e8]
    return value


def cases():
    values, labels = [], []
    for begin in (0.0, 80000.0):
        for optical in (0.0, *np.geomspace(1e-14, 1000.0, 128)):
            value = query()
            value[5, 1] = 1
            value[6, :3] = [begin, 600, optical]
            values.append(value)
            labels.append({"kind": "cell", "begin_m": begin, "optical": optical})
    for optical in (1e-8, 0.01, 0.1, 0.5, 1.0, 10.0, 1000.0):
        for count in (1, CELLS):
            value = query()
            value[5, 1] = count
            for cell in range(count):
                value[6 + cell, :3] = [
                    1000 + cell * 600 / count,
                    600 / count,
                    optical / count,
                ]
            values.append(value)
            labels.append({"kind": "subdivision", "optical": optical, "count": count})
    for limit in (999.0, 1000.0, 1050.0, 1100.0, 80000.0, 80300.0, 80600.0):
        value = query()
        value[3, 3] = limit
        value[5, 1] = 2
        value[6, :3] = [1000, 100, 0.1]
        value[7, :3] = [80000, 600, 2.0]
        values.append(value)
        labels.append({"kind": "two-depths", "limit_m": limit})
    value = query()
    value[5, 1] = 2
    value[6, :3] = [100, 0.001, -np.log(0.9)]
    value[7, :3] = [10000, 0.001, -np.log(1 - 0.8 / 0.9)]
    values.append(value)
    labels.append({"kind": "motion-counter"})
    # Field queries retain the same full physical cells before clipping a receiver.
    for cover in (0.0, 0.65, 1.0):
        layer = Layer(cover=cover)
        for height in (1772.0, 2800.0, 4000.0, 1000000.0):
            for seconds in (0.0, 37.0, 37.0 + 1 / 30):
                for direction in (
                    (0, 1, 0),
                    (0.8, 0.6, 0),
                    (0.999, 0.04, 0),
                    (0, -1, 0),
                ):
                    direction = np.asarray(direction) / np.linalg.norm(direction)
                    for limit in (1000.0, 10000.0, 1e8):
                        for east in (4500.0, 4501.0):
                            value = query(layer, seconds, origin=(east, height, -1700))
                            value[3, 3] = limit
                            value[4, :3] = direction
                            value[5, 0] = 1
                            values.append(value)
                            labels.append(
                                {
                                    "kind": "field",
                                    "cover": cover,
                                    "height_m": height,
                                    "seconds": seconds,
                                    "direction": direction.tolist(),
                                    "limit_m": limit,
                                    "east_m": east,
                                }
                            )
    return np.asarray(values), labels


def decimal_moments(cells):
    """Integrate the GPU's actual constant cells without its centroid approximation."""
    with localcontext() as context:
        context.prec = 80
        through, weight, first = Decimal(1), Decimal(0), Decimal(0)
        for begin, length, optical, _ in cells:
            if optical <= 0 or length <= 0:
                continue
            begin, length, optical = map(
                Decimal.from_float, map(float, (begin, length, optical))
            )
            decay = (-optical).exp()
            opacity = 1 - decay
            weight += through * opacity
            first += through * (begin * opacity + length * (opacity / optical - decay))
            through *= decay
        return [float(weight), float(first)]


def fixed_partitions(answers, labels):
    grouped = {}
    for index, label in enumerate(labels):
        if label["kind"] != "field":
            continue
        key = tuple(
            (name, tuple(value) if isinstance(value, list) else value)
            for name, value in label.items()
            if name != "limit_m"
        )
        grouped.setdefault(key, []).append(index)
    checks = 0
    for indices in grouped.values():
        full = answers[indices[-1], 1:]
        for index in indices[:-1]:
            row = answers[index, 1:]
            used = row[:, 3] > 0
            np.testing.assert_array_equal(row[used][:, (0, 3)], full[used][:, (0, 3)])
            np.testing.assert_allclose(
                row[used, 2] / row[used, 1],
                full[used, 2] / full[used, 1],
                rtol=5e-7,
                atol=0,
            )
            checks += int(used.sum())
    return checks


def analytic_cells(queries):
    result = np.zeros((len(queries), CELLS, 4), dtype="<f4")
    for index, value in enumerate(queries):
        if value[5, 0] != 0:
            continue
        count = int(value[5, 1])
        given = value[6 : 6 + count]
        length = np.clip(value[3, 3] - given[:, 0], 0, given[:, 1])
        result[index, :count, 0] = given[:, 0]
        result[index, :count, 1] = length
        result[index, :count, 2] = given[:, 2] * (length / given[:, 1])
        result[index, :count, 3] = given[:, 1]
    return result


def motion_changes(answers, labels):
    def key(label, seconds, east):
        return (
            label["cover"],
            label["height_m"],
            tuple(label["direction"]),
            label["limit_m"],
            seconds,
            east,
        )

    rows = {
        key(label, label["seconds"], label["east_m"]): answers[i, 0, :2]
        for i, label in enumerate(labels)
        if label["kind"] == "field"
    }
    changes = {"wind_tick": [], "translation": []}
    for label in labels:
        if (
            label["kind"] != "field"
            or label["seconds"] != 37
            or label["east_m"] != 4500
        ):
            continue
        original = rows[key(label, 37, 4500)]
        changes["wind_tick"].append(
            np.abs(rows[key(label, 37 + 1 / 30, 4500)] - original)
        )
        changes["translation"].append(np.abs(rows[key(label, 37, 4501)] - original))
    result = {name: np.max(values, axis=0).tolist() for name, values in changes.items()}
    if any(max(value) == 0 for value in result.values()):
        raise AssertionError(
            "native field moments must respond to wind tick and translation"
        )
    return result


def verify(queries, answers, labels):
    if not np.isfinite(answers).all():
        raise AssertionError("native moments/cells must stay finite")
    actual_cells = answers[:, 1:]
    expected = np.asarray([decimal_moments(row) for row in actual_cells])
    actual = answers[:, 0, :2].astype(float)
    positive = expected > 0
    relative = np.zeros_like(actual)
    relative[positive] = (
        np.abs(actual[positive] - expected[positive]) / expected[positive]
    )
    if np.max(relative) > 3e-6 or np.any(actual[~positive] != 0):
        raise AssertionError(
            f"raw moments fail the fixed FP32 cell limit: {relative.max()}"
        )
    if np.any(actual[:, 0] < 0) or np.any(actual[:, 0] > 1):
        raise AssertionError("M0 must stay in [0,1]")
    analytic = np.asarray([label["kind"] != "field" for label in labels])
    np.testing.assert_allclose(
        actual_cells[analytic], analytic_cells(queries)[analytic], rtol=3e-7, atol=0
    )
    if np.any(answers[:, 0, 2] < 0) or np.any(answers[:, 0, 2:] > CELLS):
        raise AssertionError("work counters exceed the actual bounded iterator")
    for index, label in enumerate(labels):
        if (
            label["kind"] == "field"
            and label["cover"] == 0
            and np.any(answers[index] != 0)
        ):
            raise AssertionError(
                "known-zero coverage performs no density work or extinction"
            )
    selected = [
        i
        for i, label in enumerate(labels)
        if label["kind"] == "two-depths" and label["limit_m"] in (1050, 80600)
    ]
    native_pair = actual[selected]
    filtered_depth = native_pair[:, 1].sum() / native_pair[:, 0].sum()
    reference_depth = expected[selected, 1].sum() / expected[selected, 0].sum()
    if abs(filtered_depth / reference_depth - 1) > 3e-6:
        raise AssertionError(
            "coupled native raw-moment filtering fails the independent integral"
        )
    wrong_filter = np.mean(native_pair[:, 1] / native_pair[:, 0])
    if abs(filtered_depth - wrong_filter) < 10000:
        raise AssertionError(
            "averaging divided depths must remain an explicit counterexample"
        )
    partition_checks = fixed_partitions(answers, labels)
    counter = next(
        i for i, label in enumerate(labels) if label["kind"] == "motion-counter"
    )
    cells = actual_cells[counter, :2]
    local = np.asarray([decimal_moments(row[None, :]) for row in cells])
    weights = local[:, 0] * np.exp(-np.r_[0, cells[:-1, 2].cumsum()])
    depths = local[:, 1] / local[:, 0]
    pixel_scale = 960 * 50 / 30
    layered_motion = np.sum(weights * pixel_scale / depths) / weights.sum()
    one_depth_motion = pixel_scale * actual[counter, 0] / actual[counter, 1]
    motion_error = float(layered_motion - one_depth_motion)
    if motion_error < 1.7:
        raise AssertionError(
            "the native two-layer moment must retain its motion counterexample"
        )
    return {
        "queries": len(queries),
        "raw_moment_values": int(actual.size),
        "max_relative_m0": float(relative[:, 0].max()),
        "max_relative_m1": float(relative[:, 1].max()),
        "density_evaluations": int(answers[:, 0, 2].sum()),
        "cell_visits": int(answers[:, 0, 3].sum()),
        "fixed_partition_cell_checks": partition_checks,
        "wrong_divided_filter_error_m": float(abs(filtered_depth - wrong_filter)),
        "two_layer_mean_depth_motion_error_px": motion_error,
        "max_raw_m1_m": float(actual[:, 1].max()),
        "motion_raw_moment_changes": motion_changes(answers, labels),
    }, expected
