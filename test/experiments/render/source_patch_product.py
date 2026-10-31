"""Write a diagnostic candidate using existing original native material-part files.

No cache format, native API or World publication is added. Prepared-source face
ordinals are copied from the bound originals or the source being constructed.
"""

import hashlib
import json

import numpy as np

FIELDS = ("positions", "normals", "uv", "uv1", "colours")
SLICES = (slice(0, 3), slice(3, 6), slice(6, 8), slice(8, 10), slice(10, 14))


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
