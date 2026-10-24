"""Bounded forward point-depth control. Point hits do not certify pixel coverage."""

import numpy as np


def transfer_matrix(previous_clip, current_clip, relative_shift):
    previous = np.asarray(previous_clip, dtype=np.float64)
    current = np.asarray(current_clip, dtype=np.float64)
    shift = np.asarray(relative_shift, dtype=np.float64)
    if previous.shape != (4, 4) or current.shape != (4, 4) or shift.shape != (3,):
        raise ValueError("Two clip matrices and a relative-world translation required")
    if not all(np.isfinite(v).all() for v in (previous, current, shift)):
        raise ValueError("Finite camera transform required")
    translation = np.eye(4)
    translation[:3, 3] = shift
    try:
        result = current @ translation @ np.linalg.inv(previous)
    except np.linalg.LinAlgError as error:
        raise ValueError("Previous clip matrix must be invertible") from error
    if not np.isfinite(result).all():
        raise ValueError("Nonfinite camera transfer")
    return result


def forward_points(depth, transfer, batch_size=65536):
    """Nearest target-cell splats, explicitly unsafe for final occlusion rejection.

    Reverse-Z in [0,1], zero clear; image centres have NDC y=1-2*(y+.5)/height.
    The source and target extents agree. Translation is already in transfer.
    No target-depth oracle, dilation, invented surface or hole repair is used.
    """
    depth = np.asarray(depth)
    transfer = np.asarray(transfer, dtype=np.float64)
    if (
        depth.ndim != 2
        or not depth.size
        or depth.size > 4194304
        or not np.isfinite(depth).all()
        or np.any((depth < 0) | (depth > 1))
    ):
        raise ValueError("Bounded finite reversed-Z image required")
    if transfer.shape != (4, 4) or not np.isfinite(transfer).all():
        raise ValueError("Finite homogeneous transfer required")
    if type(batch_size) is not int or not 1 <= batch_size <= 65536:
        raise ValueError("Point batches must contain 1..65536 entries")
    height, width = depth.shape
    result = np.zeros(depth.shape, dtype=np.float64)
    source_ids = np.flatnonzero(depth.ravel() > 0)
    accepted = peak_payload = 0
    for start in range(0, len(source_ids), batch_size):
        ids = source_ids[start : start + batch_size]
        y, x = np.divmod(ids, width)
        source = np.column_stack(
            (
                2 * (x + 0.5) / width - 1,
                1 - 2 * (y + 0.5) / height,
                depth.ravel()[ids],
                np.ones(len(ids)),
            )
        )
        clip = source @ transfer.T
        valid = np.isfinite(clip).all(axis=1) & (clip[:, 3] > 0)
        ndc = np.divide(
            clip[:, :3],
            clip[:, 3:],
            out=np.zeros((len(ids), 3)),
            where=valid[:, None],
        )
        screen = (ndc[:, :2] * [1, -1] + 1) * [width / 2, height / 2]
        valid &= (
            (ndc[:, 2] > 0)
            & (ndc[:, 2] <= 1)
            & (screen >= 0).all(axis=1)
            & (screen < [width, height]).all(axis=1)
        )
        target = np.floor(screen[valid]).astype(np.int64)
        np.maximum.at(result, (target[:, 1], target[:, 0]), ndc[valid, 2])
        accepted += int(valid.sum())
        peak_payload = max(
            peak_payload,
            sum(v.nbytes for v in (x, y, source, clip, ndc, screen, valid, target)),
        )
    occupied = int(np.count_nonzero(result))
    return result, {
        "source_points": len(source_ids),
        "projected_points": accepted,
        "occupied_target_pixels": occupied,
        "point_collisions": accepted - occupied,
        "result_array_bytes": result.nbytes,
        "source_id_array_bytes": source_ids.nbytes,
        "largest_explicit_batch_payload_bytes": peak_payload,
        "scope": "Point-splat diagnostic only; no certified area or final rejection. Explicit array payload excludes temporary expression/library allocations and physical copied bytes.",
    }


def depth_errors(candidate, reference):
    candidate, reference = np.asarray(candidate), np.asarray(reference)
    if candidate.shape != reference.shape:
        raise ValueError("Depth comparison requires the same target lattice")
    gap = candidate.astype(np.float64) - reference
    return {
        "candidate_covered": int(np.count_nonzero(candidate)),
        "current_covered": int(np.count_nonzero(reference)),
        "candidate_over_current_clear": int(
            np.count_nonzero((candidate > 0) & (reference == 0))
        ),
        "current_without_candidate": int(
            np.count_nonzero((reference > 0) & (candidate == 0))
        ),
        "maximum_reverse_z_overshoot": float(max(0, gap.max())),
        "overshoot_pixels_by_diagnostic_epsilon": {
            str(e): int(np.count_nonzero(gap > e)) for e in (0, 1e-6, 1e-5, 1e-4)
        },
        "scope": "Positive overshoot can falsely occlude; epsilons characterize errors, not acceptance thresholds or radiance bounds.",
    }
