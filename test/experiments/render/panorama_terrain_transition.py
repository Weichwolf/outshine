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


def overlap_far_weight(near_axial, far_axial, clip_start, overlap_end, depth_tolerance):
    if near_axial <= 0 or far_axial <= 0:
        return float(near_axial <= 0)
    near_exit = smoothstep(clip_start, overlap_end, near_axial)
    if near_exit >= 1:
        return 1.0
    far_entry = smoothstep(clip_start, overlap_end, far_axial)
    near_coverage = 1 - near_exit
    shared = far_entry / (near_coverage + far_entry)
    separation = near_axial - far_axial
    front = smoothstep(depth_tolerance, 2 * depth_tolerance, abs(separation))
    over_far = far_entry * (1 if separation > 0 else near_exit)
    over_near = near_coverage * (1 - far_entry if separation > 0 else 1)
    ordered = over_far / (over_near + over_far)
    return shared + front * (ordered - shared)


def unresolved_far_weight(near_axial, clip_start, overlap_end, opaque_at_anchor):
    if near_axial <= 0:
        return 1.0
    if not opaque_at_anchor:
        return 0.0
    return smoothstep(clip_start, overlap_end, near_axial)


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
    overlap = lambda near, far: overlap_far_weight(near, far, 250, 350, 8)
    assert overlap(140, 260) == 0
    assert overlap(270, 0) == 0
    assert overlap(0, 300) == 1
    assert overlap(300, 300) == .5
    assert overlap(351, 260) == 1
    assert overlap(305, 250) == 0
    assert .2 < overlap(310, 270) < .28
    assert overlap(270, 310) < .08
    assert overlap(349, 285) > .99
    assert unresolved_far_weight(1000, 250, 350, True) == 1
    assert unresolved_far_weight(1000, 250, 350, False) == 0
    assert unresolved_far_weight(200, 250, 350, True) == 0
    assert unresolved_far_weight(300, 250, 350, True) == .5
