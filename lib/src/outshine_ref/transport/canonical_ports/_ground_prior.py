"""Necessary prior contradictions on unchanged complete original port edges."""


def incompatible(frame, first, second, epsilon):
    from fractions import Fraction

    a, b, c, d = (Fraction(float(v)) for v in frame)
    dx, dy = (Fraction(float(second[i])) - Fraction(float(first[i])) for i in (0, 1))
    determinant = a * d - b * c
    if determinant == 0:
        return False
    qx = (d * dx - c * dy) / determinant
    qy = (a * dy - b * dx) / determinant
    required = abs(Fraction(float(second[2])) - Fraction(float(first[2])))
    required -= 2 * (Fraction(float(epsilon)) + Fraction(1, 1000000))
    gamma = Fraction(1.0000001)
    return required > 0 and required * required > gamma * gamma * (qx * qx + qy * qy)


def prior_contradiction(inputs, epsilon, budget):
    """Return an exact negative witness or no conclusion, never a feasible field.

    Original whole port endpoints are protected by finish_mesh; finite_ports
    requires their unchanged full edge with two road faces. A face may inherit
    any original frame of its part, so rejection requires every such possibility
    to fail. General triangle edges and later terminal caps grant no authority.
    The 1µm extra prior reserve makes the necessary test deliberately weaker.
    """
    limits = budget.surface
    work = {"ports": 0, "face_visits": 0, "frame_checks": 0}
    sizes = (
        len(inputs.surface.vertices),
        len(inputs.surface.triangles),
        len(inputs.part_roles),
        len(inputs.port_edges),
    )
    caps = (limits.max_vertices, limits.max_faces, limits.max_parts, limits.max_ports)
    if (
        limits.time_limit_s == 0
        or limits.solver_time_limit_s == 0
        or any(n > cap for n, cap in zip(sizes, caps))
        or sizes[1] > 4096
        or sizes[3] > 4096
    ):
        return None, {**work, "state": "skipped"}
    xyz = inputs.surface.vertices
    by_part = None
    maximum_checks = min(512, limits.max_queries)
    for port, ((a, b), parts) in enumerate(zip(inputs.port_edges, inputs.port_parts)):
        work["ports"] += 1
        first, second = xyz[a], xyz[b]
        if abs(float(second[2]) - float(first[2])) <= 2 * (epsilon + 1e-6):
            continue
        if by_part is None:
            by_part = {}
            for part, frame in zip(inputs.face_parts, inputs.grade_frames):
                work["face_visits"] += 1
                if part >= 0:
                    by_part.setdefault(int(part), set()).add(
                        tuple(map(float, frame.ravel()))
                    )
        for part in parts:
            frames = by_part.get(int(part), ())
            if not frames:
                continue
            for frame in sorted(frames):
                if work["frame_checks"] >= maximum_checks:
                    return None, {**work, "state": "work_limit"}
                work["frame_checks"] += 1
                if not incompatible(frame, first, second, epsilon):
                    break
            else:
                return {
                    "port": port,
                    "vertices": (int(a), int(b)),
                    "part": int(part),
                    "possible_frames": len(frames),
                    "prior_limit_m": epsilon,
                    "extra_prior_reserve_m": 1e-6,
                    "grade_limit": 1.0000001,
                    "proof": "exact rational squared dual-norm contradiction for every possible original parent frame of one adjacent part",
                }, {**work, "state": "contradiction"}
    return None, {**work, "state": "no_conclusion"}
