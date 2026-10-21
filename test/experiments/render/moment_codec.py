"""Premultiplied roughness-moment precision controls for #824.

Eight sampled texels come from consistent 4x4/2x2 Float32 source mips.
No geometry, normal encoding or runtime texture sampler is replaced here.
"""

import numpy as np

MODES = ("float32", "unorm8", "unorm16_rg", "fp16_moment_a8_control", "fp16_rgba")


def unorm(value, bits):
    maximum = (1 << bits) - 1
    return np.floor(
        np.clip(value, 0, 1).astype(np.float32) * maximum + np.float32(0.5)
    ).astype(np.uint32)


def encode(fields, mode):
    m, a, b = np.moveaxis(fields.astype(np.float32), -1, 0)
    words = np.zeros((*m.shape, 4), dtype="<u4")
    floating = words.view("<f4")
    floating[..., 1], floating[..., 2], floating[..., 3] = m, b, a
    if mode in (1, 2):
        q = unorm(m, 8 if mode == 1 else 16)
        floating[..., 0] = 0 if mode == 1 else (q & 255) / 255
        floating[..., 1] = q / 255 if mode == 1 else (q >> 8) / 255
        floating[..., 2], floating[..., 3] = unorm(b, 8) / 255, unorm(a, 8) / 255
    elif mode == 3:
        words[..., 0] = m.astype("<f2").view("<u2")
        floating[..., 1] = 0
        floating[..., 2], floating[..., 3] = unorm(b, 8) / 255, unorm(a, 8) / 255
    elif mode == 4:
        mh, ah, bh = [v.astype("<f2").view("<u2").astype(np.uint32) for v in (m, a, b)]
        words[..., 0] = mh << 16
        words[..., 1] = bh | (ah << 16)
        words[..., 2:] = 0
    elif mode != 0:
        raise ValueError("unknown codec")
    return words


def half(words):
    return words.astype("<u2").view("<f2").astype(float)


def decode(words, mode):
    values = words.view("<f4").astype(float)
    m, a, b = values[..., 1], values[..., 3], values[..., 2]
    if mode == 2:
        m = (256 * values[..., 1] + values[..., 0]) / 257
    elif mode == 3:
        m = half(words[..., 0])
    elif mode == 4:
        m, a, b = (
            half(words[..., 0] >> 16),
            half(words[..., 1] >> 16),
            half(words[..., 1] & 65535),
        )
    return np.stack((m, a, b), axis=-1)


def sample(level, uv):
    n = len(level)
    at = np.array(uv) * n - 0.5
    low = np.floor(at).astype(int)
    t = at - low
    pixels, weights = [], []
    for y in (0, 1):
        for x in (0, 1):
            xy = np.clip(low + [x, y], 0, n - 1)
            pixels.append(level[xy[1], xy[0]])
            weights.append((t[0] if x else 1 - t[0]) * (t[1] if y else 1 - t[1]))
    return np.array(pixels), np.array(weights)


def fixtures():
    fields, weights, metadata = [], [], []
    y, x = np.indices((4, 4))
    checker = (x + y) % 2
    for r in (0.0, 0.01, 0.02, 0.04, 0.06, 0.12, 0.2, 0.45, 0.8, 1.0):
        for coverage in (0.0, 1 / 1024, 1 / 255, 0.01, 0.1, 0.25, 0.5, 1.0):
            for pattern in ("constant", "holes", "mixed_roughness"):
                rough = np.full((4, 4), r, np.float32)
                a = np.full((4, 4), coverage, np.float32)
                if pattern == "holes":
                    a *= checker
                if pattern == "mixed_roughness":
                    rough[checker != 0] = 0.8
                alpha = rough * rough
                m = alpha * alpha * a
                # Metal channel remains independent and coverage-premultiplied.
                b = a * np.where(checker, 0.7, 0.1).astype(np.float32)
                fine = np.stack((m, a, b), axis=-1)
                coarse = fine.reshape(2, 2, 2, 2, 3).mean(axis=(1, 3), dtype=np.float32)
                for uv, lod in (
                    ((0.5, 0.5), 0.0),
                    ((0.5, 0.5), 0.5),
                    ((0.43, 0.61), 0.37),
                    ((0.125, 0.875), 1.0),
                ):
                    p0, w0 = sample(fine, uv)
                    p1, w1 = sample(coarse, uv)
                    fields.append(np.vstack((p0, p1)))
                    weights.append(np.r_[w0 * (1 - lod), w1 * lod])
                    metadata.append(
                        {
                            "roughness": r,
                            "coverage": coverage,
                            "pattern": pattern,
                            "uv": uv,
                            "lod": lod,
                        }
                    )
    return np.asarray(fields, np.float32), np.asarray(weights, np.float32), metadata
