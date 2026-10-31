"""Write a diagnostic candidate using existing original native material-part files.

No cache format, native API or World publication is added. Prepared-source face
ordinals are copied from the bound originals or the source being constructed.
"""

import hashlib
import json
import zipfile

import numpy as np

FIELDS = ("positions", "normals", "uv", "uv1", "colours")
SLICES = (slice(0, 3), slice(3, 6), slice(6, 8), slice(8, 10), slice(10, 14))


def read_product(output, record, source_ordinals):
    required = ("native_description_sha256", "prepared_source_face_binding_sha256")
    if any(key not in record for key in required):
        raise ValueError("Bound candidate metadata and actual source labels required")
    geometry = output / "geometry/0"
    description, label_path = geometry / "input.json", output / "face-sources.npz"
    if (
        not 0 < description.stat().st_size <= 65536
        or hashlib.sha256(description.read_bytes()).hexdigest() != record[required[0]]
        or not 0 < label_path.stat().st_size <= 2 * 1024**2
        or hashlib.sha256(label_path.read_bytes()).hexdigest() != record[required[1]]
    ):
        raise ValueError("Candidate description/source-label hash changed")
    parts = json.loads(description.read_text())["parts"]
    if parts != record["parts"] or len(parts) != 3:
        raise ValueError("Complete original material slot description required")
    expected_files, result = set(), []
    for material, part in enumerate(parts):
        if (
            any(
                type(part[key]) is not int
                for key in ("material", "vertices", "triangles")
            )
            or part["material"] != material
            or not 0 <= part["vertices"] <= 131072
            or not 0 <= part["triangles"] <= 131072
        ):
            raise ValueError("Bounded original native material parts required")
        values = {}
        for name, width in (*zip(FIELDS, (3, 3, 2, 2, 4), strict=True), ("indices", 3)):
            count = part["triangles" if name == "indices" else "vertices"]
            filename = f"{material}/{name}.bin"
            expected_files.add(filename)
            path = geometry / filename
            if path.stat().st_size != 4 * count * width or hashlib.sha256(
                path.read_bytes()
            ).hexdigest() != record["native_file_sha256"].get(filename):
                raise ValueError("Complete candidate native words changed: " + filename)
            values[name] = np.fromfile(
                path, "<u4" if name == "indices" else "<f4"
            ).reshape(count, width)
            if not np.isfinite(values[name]).all():
                raise ValueError("Finite complete candidate fields required")
        if values["indices"].size and values["indices"].max() >= part["vertices"]:
            raise ValueError("Candidate face outside material-part vertices")
        result.append((part, values))
    if set(record["native_file_sha256"]) != expected_files:
        raise ValueError("Exact native candidate operand inventory required")
    names = {
        f"part{i}_prepared_source_ordinals.npy": p["triangles"]
        for i, p in enumerate(parts)
    }
    with zipfile.ZipFile(label_path) as archive:
        entries = archive.infolist()
        if (
            len(entries) != 3
            or {entry.filename for entry in entries} != set(names)
            or any(
                not 0 < entry.file_size <= 4 * names[entry.filename] + 1024
                for entry in entries
            )
        ):
            raise ValueError("Bounded complete source-label arrays required")
    labels = []
    with np.load(label_path, allow_pickle=False) as saved:
        for i, part in enumerate(parts):
            value = saved[f"part{i}_prepared_source_ordinals"]
            if value.dtype != np.dtype("<i4") or value.shape != (part["triangles"],):
                raise ValueError(
                    "Exact per-triangle prepared-source label rows required"
                )
            labels.append(value)
    if not np.array_equal(
        np.unique(np.concatenate(labels)), np.sort(np.array(list(source_ordinals)))
    ):
        raise ValueError("Every bound original source must remain present")
    return tuple(
        (part, values, ids) for (part, values), ids in zip(result, labels, strict=True)
    )


def write_product(chunks, source_order, selected_sources, original_parts, output):
    if set(chunks) != set(map(int, selected_sources)) or len(original_parts) != 3:
        raise ValueError("Complete bound original source/material inventory required")
    geometry = output / "geometry/0"
    geometry.mkdir(parents=True, exist_ok=False)
    parts, face_sources, hashes = [], {}, {}
    arrays_bytes = 0
    for material in range(3):
        groups, source_ids = [], []
        for source in sorted(chunks, key=lambda s: (source_order[s], s)):
            corners, roles = chunks[source]
            values = corners[roles == material]
            if len(values):
                groups.append(values)
                source_ids.extend([source] * len(values))
        values = np.ascontiguousarray(
            np.concatenate(groups) if groups else np.empty((0, 3, 14)), dtype="<f4"
        )
        words, inverse = np.unique(
            values.reshape(-1, 14).view("<u4"), axis=0, return_inverse=True
        )
        vertices = words.view("<f4")
        faces = inverse.reshape(-1, 3).astype("<u4")
        part = {
            **original_parts[material],
            "vertices": len(vertices),
            "triangles": len(faces),
        }
        if part["material"] != material:
            raise ValueError("Existing native material slot order required")
        folder = geometry / str(material)
        folder.mkdir()
        streams = {
            key: vertices[:, columns]
            for key, columns in zip(FIELDS, SLICES, strict=True)
        }
        streams["indices"] = faces
        for name, stream in streams.items():
            path = folder / (name + ".bin")
            stream.tofile(path)
            arrays_bytes += stream.nbytes
            hashes[str(path.relative_to(geometry))] = hashlib.sha256(
                path.read_bytes()
            ).hexdigest()
        parts.append(part)
        face_sources[f"part{material}_prepared_source_ordinals"] = np.array(
            source_ids, "<i4"
        )
    if set(map(int, np.unique(np.concatenate(list(face_sources.values()))))) != set(
        chunks
    ):
        raise ValueError("No original source may disappear from the candidate")
    (geometry / "input.json").write_text(json.dumps({"parts": parts}, indent=2) + "\n")
    np.savez_compressed(output / "face-sources.npz", **face_sources)
    return {
        "parts": parts,
        "native_array_bytes": arrays_bytes,
        "native_file_sha256": hashes,
        "native_description_sha256": hashlib.sha256(
            (geometry / "input.json").read_bytes()
        ).hexdigest(),
        "prepared_source_face_binding_sha256": hashlib.sha256(
            (output / "face-sources.npz").read_bytes()
        ).hexdigest(),
        "prepared_source_face_binding_bytes": sum(
            v.nbytes for v in face_sources.values()
        ),
        "scope": "Existing NativeBuildingImpostors::Source file layout and historical three material slots. Diagnostic candidate only; duplicate vertices share identical full position/normal/UV/UV1/colour words. Tangent/other shaders and runtime World use are not qualified.",
    }
