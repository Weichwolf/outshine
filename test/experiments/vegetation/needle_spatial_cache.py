"""Atomic experimental patch cache; original geometry and identity stay attached."""

import json

import numpy as np


def save_product(path, source, products, remainder):
    arrays = {f"source_{k}": v for k, v in source.items() if isinstance(v, np.ndarray)}
    metadata = {k: v for k, v in source.items() if not isinstance(v, np.ndarray)}
    arrays.update(
        schema=np.array("needle-spatial-v1"),
        metadata=np.array(json.dumps(metadata)),
        charts=np.array(len(products)),
        remainder=remainder,
    )
    for index, product in enumerate(products):
        for key, value in product["patch"].items():
            arrays[f"patch_{index}_{key}"] = np.asarray(value)
        for side, view in enumerate(product["views"]):
            for key, value in view.items():
                arrays[f"view_{index}_{side}_{key}"] = np.asarray(value)
    temporary = path.with_suffix(".tmp.npz")
    np.savez_compressed(temporary, **arrays)
    temporary.replace(path)


def load_product(path, expected_identity):
    with np.load(path, allow_pickle=False) as stored:
        if str(stored["schema"]) != "needle-spatial-v1":
            raise ValueError("unsupported experimental spatial product")
        metadata = json.loads(str(stored["metadata"]))
        if metadata["identity"] != expected_identity:
            raise ValueError("cached spatial plan identity mismatch")
        count = int(stored["charts"])
        if not 0 <= count <= 256:
            raise ValueError("bounded cached chart count required")
        arrays = {
            key: stored[key]
            for key in stored.files
            if key not in ("schema", "metadata", "charts")
        }
    for value in arrays.values():
        value.setflags(write=False)
    source = metadata | {
        key.removeprefix("source_"): value
        for key, value in arrays.items()
        if key.startswith("source_")
    }
    source["ids"] = [tuple(i) for i in source["ids"]]
    products = []
    for index in range(count):
        prefix = f"patch_{index}_"
        patch = {
            key.removeprefix(prefix): value
            for key, value in arrays.items()
            if key.startswith(prefix)
        }
        for key in ("offset_m", "displacement_m"):
            patch[key] = float(patch[key])
        views = []
        for side in (0, 1):
            prefix = f"view_{index}_{side}_"
            view = {
                key.removeprefix(prefix): value
                for key, value in arrays.items()
                if key.startswith(prefix)
            }
            view["pixel_area_m2"] = float(view["pixel_area_m2"])
            views.append(view)
        products.append({"patch": patch, "views": views})
    return source, products, arrays["remainder"]
