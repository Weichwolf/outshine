"""Adapt original connected carriers to an explicit constant native tangent frame."""

import json
import struct

import numpy as np


def normal_directions(encoded):
    encoded = np.asarray(encoded, dtype=float)
    if encoded.ndim < 1 or encoded.shape[-1] != 4 or not np.isfinite(encoded).all():
        raise ValueError("finite premultiplied RGBA normal moments required")
    mass = encoded[..., 3]
    moment = 2 * encoded[..., :3] - mass[..., None]
    length = np.linalg.norm(moment, axis=-1)
    if np.any((mass < 0) | (mass > 1) | (length > mass + 1e-12)):
        raise ValueError("normal moment exceeds its coverage mass")
    if np.any((mass > 0) & (length <= 1e-12)):
        raise ValueError("a cancelling normal distribution has no single direction")
    direction = moment / np.maximum(length[..., None], 1e-12)
    direction[length == 0] = (0, 0, 1)
    return direction, length


def export_carriers(source, output, segments):
    from connected_leaf import build_connected

    size = 64
    packet = bytearray(struct.pack("<QII", 0x31524643464C, size, len(source.blades)))
    report = []
    for variant, blade in enumerate(source.blades):
        product = build_connected(blade, variant, size=size, segments=segments)
        mesh = product["mesh"]
        encoded = product["normal"]
        mass = encoded[..., 3]
        direction, length = normal_directions(encoded)
        normal = np.concatenate(
            ((direction + 1) / 2, np.ones_like(mass)[..., None]), axis=-1
        )
        rgba = np.ones_like(normal)
        rgba[..., 3] = mass
        normal_bytes = np.rint(np.clip(normal, 0, 1) * 255).astype(np.uint8)
        colour_bytes = np.rint(np.clip(rgba, 0, 1) * 255).astype(np.uint8)
        restored = normal_bytes[..., :3].astype(float) * (2 / 255) - 1
        restored /= np.linalg.norm(restored, axis=-1, keepdims=True)
        occupied = mass > 0
        decoded_error = np.linalg.norm(restored - direction, axis=-1)
        mean_loss = 1 - length / np.maximum(mass, 1e-12)
        for array, dtype in (
            (mesh["vertices"], "<f4"),
            (mesh["uv"], "<f4"),
            (mesh["faces"], "<u4"),
        ):
            array = np.asarray(array, dtype=dtype).ravel()
            packet += struct.pack("<I", array.size) + array.tobytes()
        packet += colour_bytes.tobytes() + normal_bytes.tobytes()
        report.append(
            {
                "variant": variant,
                "carrier_identity": product["identity"],
                "vertices": len(mesh["vertices"]),
                "triangles": len(mesh["faces"]),
                "source_surface_bound_m": product["paired_source_surface_bound_m"],
                "normalized_mean_loss_max": float(mean_loss[occupied].max()),
                "normalized_mean_loss_area_mean": float(
                    (mean_loss * mass).sum() / mass.sum()
                ),
                "normal_quantization_vector_error_max": float(
                    decoded_error[occupied].max()
                ),
                "alpha_mean": float(mass.mean()),
                "encoded_alpha_mean": float(colour_bytes[..., 3].mean() / 255),
            }
        )
    (output / "carriers.input").write_bytes(packet)
    (output / "carriers.json").write_text(
        json.dumps(
            {
                "source_catalog": source.identity,
                "adaptation": "normalized conditional object mean; explicit N=+Z T=+X tangent frame",
                "colour": "constant native thin-sheet R/T; straight white/coverage image",
                "size": size,
                "segments": segments,
                "variants": report,
            },
            indent=2,
        )
        + "\n"
    )
    return "carriers.input"
