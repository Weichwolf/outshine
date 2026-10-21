"""Independent decimal GGX reference, including Float32 range diagnostics (#829)."""

from decimal import Decimal, localcontext
from itertools import product

import numpy as np

PI = Decimal(
    "3.141592653589793238462643383279502884197169399375105820974944592307816406286208998628034825342117068"
)
CHANNELS = ("distribution", "visibility", "lobe", "specular")


def decimal(value):
    """Preserve the exact binary input rather than its short decimal spelling."""
    return value if isinstance(value, Decimal) else Decimal.from_float(float(value))


def evaluate(a2, nl, nv, nh, fresnel=1, energy=1):
    """Return D, V, D*V and F*D*V*energy*nl in unbounded exponent arithmetic.

    Use the unoptimised Smith lambda formulation as an independent check on
    the product's correlated visibility expression. Zero a2 follows the product
    lobe guard; it is not a finite-width model of an ideal delta reflector.
    """
    with localcontext() as context:
        # Also covers exact r^4 for every positive Float32 r, down to ~1e-180.
        context.prec = 300
        q, light, view, half, f, e = map(decimal, (a2, nl, nv, nh, fresnel, energy))
        if not (0 <= q <= 1 and 0 < light <= 1 and 0 < view <= 1 and 0 <= half <= 1):
            raise ValueError("GGX scalar inputs outside the declared unit domain")
        d = q / (PI * (half * half * (q - 1) + 1) ** 2) if q else Decimal(0)

        def smith_lambda(cosine):
            return ((1 + q * (1 - cosine**2) / cosine**2).sqrt() - 1) / 2

        visibility = 1 / (
            (1 + smith_lambda(light) + smith_lambda(view)) * 4 * light * view
        )
        lobe = d * visibility
        return d, visibility, lobe, f * lobe * e * light


def row_reference(row, captured_a2=None, energy=1):
    """Use captured q or exact represented-r^4; bound cosine overshoots explicitly."""
    with localcontext() as context:
        context.prec = 300
        r, nh, nl, nv, vh, f0, explicit, mode = map(decimal, row)
        q = (
            (explicit if mode else r**4)
            if captured_a2 is None
            else decimal(captured_a2)
        )
        f = f0 + (1 - f0) * (1 - vh) ** 5
        return evaluate(q, nl, nv, min(nh, 1), f, energy)


def fixtures():
    """Scalar-function matrix, plus physical normal/mirror and rounding controls.

    Rows contain r, nh, nl, nv, vh, f0, explicit-a2, use-explicit-a2. Arbitrary
    scalar tuples are primitive tests, not claims of realisable light/view poses.
    """
    roughness = (
        0,
        1e-12,
        1e-10,
        1e-8,
        1e-6,
        1e-4,
        0.001,
        0.01,
        0.02,
        0.12,
        0.2,
        0.5,
        1,
    )
    half = (1, np.nextafter(np.float32(1), np.float32(0)), 0.9999, 0.99, 0.9, 0)
    rows, groups = [], []
    for r, nh, cosine, f0 in product(roughness, half, (1, 0.5, 1e-6), (0, 0.04, 1)):
        rows.append((r, nh, cosine, cosine, 1, f0, 0, 0))
        groups.append("normal_incidence" if nh == cosine == 1 else "scalar_primitive")
    # N bisects mirror-symmetric L,V: nh=1 and vh=nl=nv are a real grazing pose.
    for r, cosine in product((1e-10, 1e-8, 0.01, 0.12, 0.5), (0.5, 1e-6)):
        rows.append((r, 1, cosine, cosine, cosine, 0.04, 0, 0))
        groups.append("mirror_symmetric")
    # Explicit q separates representability of GGX from r^4 source underflow.
    small = np.finfo(np.float32)
    for q, f0 in product(
        (small.smallest_subnormal, 1e-40, small.tiny, 1e-36, 1e-30), (0, 0.04, 1)
    ):
        rows.append((0.01, 1, 1, 1, 1, f0, q, 1))
        groups.append("explicit_a2")
    for r in (0.01, 0.02, 0.12):
        rows.append(
            (r, np.nextafter(np.float32(1), np.float32(2)), 1, 1, 1, 0.04, 0, 0)
        )
        groups.append("cosine_rounding_to_physical_one")
    return np.asarray(rows, dtype="<f4"), groups


def compare(actual, expected):
    """Separate range classes; never hide nonfinite values behind finite masks."""
    actual, expected = (
        np.asarray(actual, dtype=float),
        np.asarray(expected, dtype=float),
    )
    limits = np.finfo(np.float32)
    finite = np.isfinite(actual)
    normal = (abs(expected) >= limits.tiny) & (abs(expected) <= limits.max)
    zero = expected == 0
    subnormal = (abs(expected) < limits.tiny) & ~zero
    outside = abs(expected) > limits.max
    relative = np.full_like(expected, np.inf)
    np.divide(abs(actual - expected), abs(expected), out=relative, where=finite & ~zero)
    relative[zero & (actual == 0)] = 0
    bad = (~finite | (relative > 64 * limits.eps)) & (normal | zero)
    return {
        "total": len(expected),
        "nonfinite": int((~finite).sum()),
        "reference_normal_or_zero": int((normal | zero).sum()),
        "reference_subnormal": int(subnormal.sum()),
        "reference_outside_float32": int(outside.sum()),
        "subnormal_reference_nonfinite": int((subnormal & ~finite).sum()),
        "subnormal_reference_zero": int((subnormal & (actual == 0)).sum()),
        "outside_reference_finite": int((outside & finite).sum()),
        "bad_representable": int(bad.sum()),
        "bad_indices": np.flatnonzero(bad).tolist(),
        "finite_normal_max_relative": float(relative[normal & finite].max(initial=0)),
    }
