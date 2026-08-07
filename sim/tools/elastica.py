#!/usr/bin/env python3
"""The blade's own bending equation, solved, and the closed form GroundCoverStage bakes.

Boundary-value problem of Gosselin, de Langre & Machado-Almeida (2010), JFM 650:319-341, eq.
(5.5)/(5.7) -- a uniform cantilever standing normal to the flow, loaded by the cross-flow momentum
of the stream it deflects:

    d3(theta)/ds3 = Cy * cos^2(theta)      theta(0) = 0, theta'(1) = theta''(1) = 0

s is arc length over the blade length; Cy = rho*Cd*L^3*U^2/(2*B) is the SCALED Cauchy number, the
one dimensionless group the problem has.  Two results leave this file:

  * the TIP angle Theta(Cy) as a closed form whose two ends are derived, not fitted -- slope Cy/6
    at zero load (the linear cantilever) and pi/2 at infinite load (the blade lies in the flow);
  * the SHAPE theta(s)/Theta, which needs no fit at all: 1 - (1-s)^3 holds over the whole family.

The Vogel exponent of the solved drag is printed as the check against Table 1 of the same paper,
which gives V = -2/3 for a plate or a fibre in the large-deformation asymptote.
"""

import math

NS = 2001
KNEE = (1.09246, -2.01180, 0.19703)


def solve(cy, guess=None):
    """Shoot on (theta'(0), theta''(0)); returns theta(s) on a uniform grid."""
    h = 1.0 / (NS - 1)

    def march(p, q):
        th, d1, d2 = 0.0, p, q
        out = [0.0]
        for _ in range(NS - 1):
            k = []
            y = (th, d1, d2)
            for a, b in ((0.0, None), (0.5, 0), (0.5, 1), (1.0, 2)):
                z = y if b is None else tuple(y[i] + h * a * k[b][i] for i in range(3))
                k.append((z[1], z[2], cy * math.cos(z[0]) ** 2))
            th += h / 6.0 * (k[0][0] + 2 * k[1][0] + 2 * k[2][0] + k[3][0])
            d1 += h / 6.0 * (k[0][1] + 2 * k[1][1] + 2 * k[2][1] + k[3][1])
            d2 += h / 6.0 * (k[0][2] + 2 * k[1][2] + 2 * k[2][2] + k[3][2])
            out.append(th)
        return out, d1, d2

    p, q = guess if guess else (cy / 3.0, -cy)
    for _ in range(400):
        _, f1, f2 = march(p, q)
        if abs(f1) < 1e-12 and abs(f2) < 1e-12:
            break
        e = 1e-7
        _, a1, a2 = march(p + e, q)
        _, b1, b2 = march(p, q + e)
        j = ((a1 - f1) / e, (b1 - f1) / e, (a2 - f2) / e, (b2 - f2) / e)
        det = j[0] * j[3] - j[1] * j[2]
        if abs(det) < 1e-18:
            break
        dp = (-f1 * j[3] + f2 * j[1]) / det
        dq = (-j[0] * f2 + j[2] * f1) / det
        r = 1.0
        while r > 1e-4:
            _, n1, n2 = march(p + r * dp, q + r * dq)
            if abs(n1) + abs(n2) < abs(f1) + abs(f2):
                break
            r *= 0.5
        p += r * dp
        q += r * dq
    th, _, _ = march(p, q)
    return th, (p, q)


def reconfiguration(th):
    """F / (0.5*rho*Cd*W*L*U^2) -- the reconfiguration number R of the same paper."""
    return sum(math.cos(t) ** 3 for t in th) / (NS - 1.0)


def tip_fit(cy):
    """WHAT THE SHADER RUNS.  x carries the linear cantilever exactly, t bounds it, and the cubic in
    t vanishes at both ends, so neither asymptote can be moved by the three coefficients."""
    x = cy / (3.0 * math.pi)
    t = x / (1.0 + x)
    return 0.5 * math.pi * (t + (1.0 - t) * t * t * (KNEE[0] + t * (KNEE[1] + t * KNEE[2])))


def main():
    cys = [0.02 * 1.12 ** i for i in range(69)]   # 0.02 .. 44.4; 44 is a hurricane on this canopy
    rows, guess = [], None
    for cy in cys:
        th, guess = solve(cy, guess)
        rows.append((cy, th, reconfiguration(th)))

    print("  Cy        tip(deg)   fit(deg)   err(deg)   shape n  shape res   R")
    tip_abs = tip_rel = shape_res = 0.0
    for i, (cy, th, r) in enumerate(rows):
        tip = th[-1]
        tip_abs = max(tip_abs, abs(tip_fit(cy) - tip))
        tip_rel = max(tip_rel, abs(tip_fit(cy) / tip - 1.0))
        num = den = 0.0
        for j in range(1, NS - 1):
            s = j / (NS - 1.0)
            g = th[j] / tip
            if 0.0 < g < 1.0:
                num += math.log(1.0 - g) * math.log(1.0 - s)
                den += math.log(1.0 - s) ** 2
        res = max(abs(th[j] / tip - (1.0 - (1.0 - j / (NS - 1.0)) ** 3)) for j in range(NS))
        shape_res = max(shape_res, res)
        if i % 6 == 0:
            print("%8.4f  %8.3f  %8.3f  %8.3f   %7.4f  %9.5f  %6.4f"
                  % (cy, math.degrees(tip), math.degrees(tip_fit(cy)),
                     math.degrees(tip_fit(cy) - tip), num / den, res, r))

    print()
    print("tip    Theta = (pi/2)*(t + (1-t)*t^2*(%.5f + %.5f*t + %.5f*t^2)),  t = x/(1+x)" % KNEE)
    print("       x = Cy/(3*pi).  max |err| %.4f deg, max relative %.5f, over Cy %.2f..%.1f"
          % (math.degrees(tip_abs), tip_rel, cys[0], cys[-1]))
    print("shape  theta(s)/Theta = 1 - (1-s)^3, no free parameter.  max |err| %.5f" % shape_res)

    print()
    print("Vogel exponent V of the solved drag, F ~ U^(2+V) -- Gosselin et al. Table 1 gives -2/3")
    n = len(rows)
    for lo, hi in ((0, 8), (n // 3, n // 3 + 16), (n - 25, n - 9), (n - 17, n - 1)):
        c0, r0 = rows[lo][0], rows[lo][2]
        c1, r1 = rows[hi][0], rows[hi][2]
        print("  Cy %7.3f -> %7.3f   V = %+.3f"
              % (c0, c1, math.log(r1 / r0) / math.log(math.sqrt(c1 / c0))))


if __name__ == "__main__":
    main()
