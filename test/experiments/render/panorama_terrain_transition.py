"""Reference ownership between cached terrain geometry and a clipped far replica."""


def smoothstep(start, end, value):
    t = min(1, max(0, (value - start) / (end - start)))
    return t * t * (3 - 2 * t)


def far_weight(near_axial, far_axial, clip_start, face_px):
    if near_axial <= 0:
        return 1
    if far_axial <= 0:
        return 0
    if clip_start <= 0:
        return int(near_axial >= far_axial)
    # Eight texels across a face whose span is twice the axial depth.
    width = 16 * clip_start / face_px
    coverage = smoothstep(clip_start, clip_start + width, near_axial)
    clearly_in_front = smoothstep(width, 2 * width, near_axial - far_axial)
    return max(coverage, clearly_in_front)


if __name__ == "__main__":
    assert far_weight(2, 10, 0, 2) == 0
    assert far_weight(20, 10, 0, 2) == 1
    assert far_weight(1250, 0, 1200, 512) == 0
    assert far_weight(0, 1230, 1200, 512) == 1
    assert far_weight(1220, 1155, 1200, 512) > far_weight(1220, 1221, 1200, 512)
    assert far_weight(1300, 1298, 1200, 512) == 1
    for depth in (1200, 1210, 1220, 1230, 1240):
        weights = {far_weight(depth, depth + offset, 1200, 512) for offset in (-6, -1, 0, 1, 6)}
        assert len(weights) == 1
