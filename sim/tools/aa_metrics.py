#!/usr/bin/env python3
"""The four numbers an antialiasing round is judged on, one instrument for all of them.

  tools/aa_metrics.py edges   A.png [B.png ...] [--step 40]
  tools/aa_metrics.py spans   A.png [B.png ...]
  tools/aa_metrics.py laplace A.png A.f32 [B.png B.f32 ...] [--fov 60]
  tools/aa_metrics.py ref     REF.png A.png [B.png ...]
  tools/aa_metrics.py ghost   NOTAA_DIR TAA_DIR --frame N
  tools/aa_metrics.py shift   A.png B.png A.f32 [--fov 60]

Every definition is stated where it is computed; the numbers are only comparable between images
measured by THIS file, which is why it is one file and not four scripts.
"""

import argparse
import sys

import numpy as np
from PIL import Image

YW = np.array([0.2126, 0.7152, 0.0722])
NEAR_M = 0.05           # MvpCamRel's zn — the numerator of range = zn / depth / cos(off-axis)


def rgb(path):
    return np.asarray(Image.open(path).convert("RGB"), dtype=np.float64)


def luma8(path):
    """Display luminance in CODES (0..255), the quantity a step of "40 codes" is counted in."""
    c = rgb(path)
    return YW[0] * c[..., 0] + YW[1] * c[..., 1] + YW[2] * c[..., 2]


def depth_range(f32path, w, h, fov_deg):
    """Range along the view ray per pixel, metres, from the reversed-Z scene depth.

    depth = zn / (-z_eye) and the ray leaves the boresight, so the distance ALONG THE RAY is
    zn/depth divided by the cosine of the off-axis angle. Sky (depth 0) comes back as +inf."""
    d = np.fromfile(f32path, dtype=np.float32).reshape(h, w).astype(np.float64)
    f = 0.5 * h / np.tan(0.5 * np.radians(fov_deg))
    ys, xs = np.mgrid[0:h, 0:w]
    dx = xs + 0.5 - 0.5 * w
    dy = 0.5 * h - (ys + 0.5)
    cos_off = f / np.sqrt(dx * dx + dy * dy + f * f)
    with np.errstate(divide="ignore"):
        return np.where(d > 0.0, NEAR_M / np.maximum(d, 1e-30) / cos_off, np.inf)


def cmd_edges(args):
    """THE EDGE POPULATION: what share of horizontal neighbour pairs jumps more than `step` display
    codes with no value in between. A full-contrast edge without an intermediate sample is exactly
    what a one-sample-per-pixel rasteriser produces and what more samples per pixel remove."""
    print("horizontal neighbour pairs with |dY| > %d codes" % args.step)
    for p in args.images:
        y = luma8(p)
        d = np.abs(np.diff(y, axis=1))
        print("  %-44s %8d / %8d  %6.3f %%" % (p, int((d > args.step).sum()), d.size,
                                               100.0 * (d > args.step).mean()))


def sky_envelope(sky):
    """The picture's SKY REGION: every row above the lowest sky pixel of its own column. Inside it a
    non-sky pixel is a plant and outside it the question does not arise, which is what lets the two
    statements below be made without a second render or a hand-drawn mask."""
    h, w = sky.shape
    ys = np.arange(h)[:, None]
    last = np.where(sky.any(axis=0), (h - 1) - np.argmax(sky[::-1], axis=0), -1)
    return ys <= last[None, :]


def fragments(sky, env):
    """WHAT THE RUN HISTOGRAM CANNOT SAY: how many PIECES the picture is in.

    A blade drawn continuously is ONE 8-connected non-sky component; the same blade rendered one
    sample per pixel is a chain of separate dots, and each dot is a component of its own. Counting
    the components that never touch the picture's edge therefore counts the dashes directly, with no
    dependence on how a run happens to be split by a threshold. Union-find over a scanline pass, so
    it costs one pass and no library."""
    m = (~sky) & env
    h, w = m.shape
    parent = {}

    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a

    def union(a, b):
        ra, rb = find(a), find(b)
        if ra != rb:
            parent[rb] = ra

    lab = np.zeros((h, w), dtype=np.int64)
    nxt = 1
    for y in range(h):
        for x in np.flatnonzero(m[y]):
            ns = []
            if x > 0 and lab[y, x - 1]:
                ns.append(lab[y, x - 1])
            if y > 0:
                for dx in (-1, 0, 1):
                    if 0 <= x + dx < w and lab[y - 1, x + dx]:
                        ns.append(lab[y - 1, x + dx])
            if not ns:
                lab[y, x] = nxt
                parent[nxt] = nxt
                nxt += 1
            else:
                lab[y, x] = ns[0]
                for n in ns[1:]:
                    union(ns[0], n)
    roots = {}
    border = set()
    for y in range(h):
        for x in np.flatnonzero(m[y]):
            r = find(lab[y, x])
            roots[r] = roots.get(r, 0) + 1
            if x == 0 or y == 0 or x == w - 1 or y == h - 1:
                border.add(r)
    return sum(1 for r in roots if r not in border)


def ink_over_sky(path, sky, env):
    """THE COVERAGE THAT REACHED THE PICTURE, threshold-free: how far the SKY REGION departs from the
    sky's own colour, summed over every pixel in it. A sub-pixel blade that was dropped contributes
    nothing; the same blade resolved as a partial coverage contributes its share. It has to HOLD or
    RISE where the fragment count falls, or the filter merely erased the blade."""
    y = luma8(path)
    med = float(np.median(y[sky])) if sky.any() else 0.0
    return float(np.abs(y[env] - med).sum() / 1000.0)


def cmd_spans(args):
    """THE GEOMETRY LOSS: how wide a blade is where it crosses the SKY, by horizontal run length.

    The sky is the one background in the bench frame that needs no second render to identify — it is
    the only blue thing in it — so a maximal run of non-sky pixels with sky on BOTH sides is a blade
    crossing and its length is the blade's width in pixels at that row. A blade narrower than a
    sample is drawn in segments: it is present on the rows where the pixel centre happens to fall
    inside it and absent between, and those rows are exactly the 1 px and 2 px runs.

    Reproduces the art director's published 643 / 498 on `wiese-b-frontlit` to 648 / 500 (the
    residual is his sky threshold, which was not published) and is bit-identical on that file before
    and after the keel round, as he stated."""
    print("blade crossings of the sky, by width in px (sky = B-R > %d and B > %d)"
          % (args.sky_diff, args.sky_min))
    env = None
    for p in args.images:
        c = rgb(p).astype(np.int32)
        sky = (c[..., 2] - c[..., 0] > args.sky_diff) & (c[..., 2] > args.sky_min)
        # ONE region for every image in the comparison, taken from the FIRST: an envelope of its own
        # would move with the picture, and a filter that closes a sky hole deep in the canopy would
        # then be credited with removing the dark canopy the hole exposed.
        if env is None:
            env = sky_envelope(sky)
        hist = {}
        for row in sky:
            idx = np.flatnonzero(np.diff(np.concatenate(([0], (~row).view(np.int8), [0]))))
            for a, b in zip(idx[0::2], idx[1::2]):
                if a == 0 or b == len(row) or not (row[a - 1] and row[b]):
                    continue
                n = int(b - a)
                hist[n] = hist.get(n, 0) + 1
        tot = sum(hist.values())
        print("  %-40s skypx=%7d  runs=%6d  1px=%5d  2px=%5d  3px=%5d  >=4px=%6d" %
              (p.split("/")[-1], int(sky.sum()), tot, hist.get(1, 0), hist.get(2, 0),
               hist.get(3, 0), tot - hist.get(1, 0) - hist.get(2, 0) - hist.get(3, 0)))
        # THE PATHOLOGY IS THE GAP, NOT THE WIDTH. A sub-pixel blade drawn CORRECTLY is one pixel
        # wide, so the 1 px population can never go to zero; what must go is the interruption. The
        # sweep counts enclosed non-background pieces at a range of detection sensitivities, so no
        # single threshold decides the answer.
        y = luma8(p)
        med = float(np.median(y[sky])) if sky.any() else 0.0
        sweep = "  ".join("%d:%d" % (t, fragments(np.abs(y - med) <= t, env))
                          for t in (2, 4, 8, 16, 32, 48))
        print("      %-36s ink=%9.1f codes   fragments by threshold  %s" %
              ("", ink_over_sky(p, sky, env), sweep))


LAPLACE_BANDS = [(0, 3), (3, 8), (8, 15), (15, 25), (25, 35), (35, 44), (44, 80)]


def cmd_laplace(args):
    """|LAPLACE| BY DISTANCE BAND. The four-neighbour Laplacian of display luminance normalised to
    [0,1]: |4Y - (Yl+Yr+Yu+Yd)|. It is the isotropic second difference, so it answers to a lone
    bright pixel among dark ones — the signature of a blade rendered one sample per pixel — and not
    to a smooth gradient. The bands are metres of RANGE along the view ray out of the depth buffer,
    so "at 8-15 m" is a mask and not a guess about a row."""
    pairs = list(zip(args.images[0::2], args.images[1::2]))
    hdr = "  %-30s" % "band [m]" + "".join("%14s" % p.split("/")[-1] for p, _ in pairs)
    cols = {}
    for png, f32 in pairs:
        y = luma8(png) / 255.0
        h, w = y.shape
        lap = np.abs(4.0 * y[1:-1, 1:-1] - y[:-2, 1:-1] - y[2:, 1:-1] - y[1:-1, :-2] - y[1:-1, 2:])
        r = depth_range(f32, w, h, args.fov)[1:-1, 1:-1]
        cols[png] = [(lap[(r >= a) & (r < b)].mean() if ((r >= a) & (r < b)).any() else float("nan"),
                      int(((r >= a) & (r < b)).sum())) for a, b in LAPLACE_BANDS]
        cols[png].append((lap[np.isfinite(r)].mean(), int(np.isfinite(r).sum())))
        cols[png].append(((lap > 0.10).mean(), lap.size))
    print(hdr)
    names = [b for b in ["%g-%g m" % t for t in LAPLACE_BANDS]] + ["all ground", "share > 0.10"]
    for i, nm in enumerate(names):
        print("  %-30s" % nm + "".join("%14.4f" % cols[p][i][0] for p, _ in pairs))
    print("  %-30s" % "px in band (first image)" +
          "".join("%14d" % cols[pairs[0][0]][i][1] for i in [0]))


def radial_power(y):
    """Power in three radial frequency bands of the 2-D spectrum, in cycles per pixel:

        low  0.00-0.20   the picture's composition — must not move
        mid  0.20-0.35   genuine detail at the resolution limit — this is the sharpness at issue
        top  0.35-0.50   the Nyquist octave, where a one-sample-per-pixel rasteriser dumps the
                         frequencies it could not represent. An excess here IS the aliasing

    Separating the two is what makes "sharpness" answerable at all: an aliased frame carries MORE
    gradient energy than the truth, so a single gradient number cannot tell blur from correctness."""
    f = np.fft.fftshift(np.abs(np.fft.fft2(y - y.mean())) ** 2)
    h, w = y.shape
    fy = np.fft.fftshift(np.fft.fftfreq(h))[:, None]
    fx = np.fft.fftshift(np.fft.fftfreq(w))[None, :]
    r = np.hypot(fx, fy)
    return [float(f[(r >= a) & (r < b)].mean()) for a, b in ((0.0, 0.20), (0.20, 0.35), (0.35, 0.51))]


def cmd_ref(args):
    """AGAINST A GROUND TRUTH. The reference is a supersampled render box-filtered down, i.e. the
    coverage the frame is trying to estimate. Two numbers per image: how far it is from the truth
    (RMSE in codes) and how much high-frequency energy it carries relative to the truth (mean
    gradient magnitude). Below 1.00 on the second is softening, above 1.00 is aliasing."""
    ref = luma8(args.reference)
    gref = np.hypot(np.gradient(ref, axis=0), np.gradient(ref, axis=1)).mean()
    bref = radial_power(ref)
    print("reference %s" % args.reference)
    print("  mean |grad| = %.4f codes/px   band power  low %.4g  mid %.4g  top %.4g"
          % (gref, bref[0], bref[1], bref[2]))
    for p in args.images:
        y = luma8(p)
        g = np.hypot(np.gradient(y, axis=0), np.gradient(y, axis=1)).mean()
        rmse = float(np.sqrt(((y - ref) ** 2).mean()))
        b = radial_power(y)
        print("  %-30s RMSE %7.3f   |grad| %7.4f = %.3f x   low %.3f x  mid %.3f x  top %.3f x"
              % (p.split("/")[-1], rmse, g, g / gref,
                 b[0] / bref[0], b[1] / bref[1], b[2] / bref[2]))


def cmd_ghost(args):
    """GHOSTING, as a correlation and not as an impression.

    A temporal filter that trails leaves the PREVIOUS frame's content in the current one. So take the
    residual the filter introduces, r = TAA_k - NOTAA_k, and the step the scene made, s =
    NOTAA_(k-1) - NOTAA_k. If the filter is trailing, r contains a share of s and the normalised
    correlation <r,s>/|r||s| is positive; if it is only antialiasing, r is the sub-pixel detail the
    single-sample render missed and is uncorrelated with the step. The projection <r,s>/<s,s> is the
    same statement as a FRACTION OF A FRAME of lag."""
    k = args.frame
    a = luma8("%s/%04d.png" % (args.taa, k))
    b = luma8("%s/%04d.png" % (args.notaa, k))
    p = luma8("%s/%04d.png" % (args.notaa, k - 1))
    r = (a - b).ravel()
    s = (p - b).ravel()
    r -= r.mean()
    s -= s.mean()
    corr = float(r @ s / (np.linalg.norm(r) * np.linalg.norm(s)))
    lag = float(r @ s / (s @ s))
    print("frame %d   |TAA-NOTAA| mean %.3f codes   |step| mean %.3f codes" %
          (k, np.abs(a - b).mean(), np.abs(p - b).mean()))
    print("  correlation(residual, previous-frame step) = %+.4f" % corr)
    print("  projection  (residual on step)             = %+.4f frames of lag" % lag)


def cmd_shift(args):
    """IS THE JITTER A CAMERA PROPERTY? Two frames that differ only in the pinned sub-pixel phase
    must differ by a RIGID translation of exactly that phase — the same shift at three metres and at
    forty. A shift that grows or shrinks with range is parallax, i.e. something world-fixed moved.

    Measured per distance band by the sub-pixel peak of the normalised cross-correlation over
    horizontal lags, on the band's own high-passed luminance."""
    a = luma8(args.a)
    b = luma8(args.b)
    h, w = a.shape
    r = depth_range(args.depth, w, h, args.fov)
    for lo, hi in LAPLACE_BANDS:
        m = (r >= lo) & (r < hi)
        if m.sum() < 5000:
            continue
        rows = np.flatnonzero(m.any(axis=1))
        best, vals = None, []
        for lag in (-2, -1, 0, 1, 2):
            bb = np.roll(b, lag, axis=1)
            mm = m & np.roll(m, lag, axis=1)
            x = a[mm] - a[mm].mean()
            yv = bb[mm] - bb[mm].mean()
            c = float(x @ yv / (np.linalg.norm(x) * np.linalg.norm(yv)))
            vals.append(c)
        i = int(np.argmax(vals))
        sub = 0.0
        if 0 < i < 4:
            d1, d2, d3 = vals[i - 1], vals[i], vals[i + 1]
            den = d1 - 2 * d2 + d3
            sub = 0.5 * (d1 - d3) / den if abs(den) > 1e-12 else 0.0
        print("  %5.0f-%3.0f m  n=%8d  rows %4d..%4d  peak lag %+.3f px  r=%.4f" %
              (lo, hi, int(m.sum()), rows[0], rows[-1], (i - 2) + sub, vals[i]))


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)

    e = sub.add_parser("edges"); e.add_argument("images", nargs="+")
    e.add_argument("--step", type=float, default=40.0); e.set_defaults(fn=cmd_edges)

    s = sub.add_parser("spans"); s.add_argument("images", nargs="+")
    s.add_argument("--sky-diff", type=int, default=20)
    s.add_argument("--sky-min", type=int, default=120); s.set_defaults(fn=cmd_spans)

    l = sub.add_parser("laplace"); l.add_argument("images", nargs="+")
    l.add_argument("--fov", type=float, default=60.0); l.set_defaults(fn=cmd_laplace)

    r = sub.add_parser("ref"); r.add_argument("reference"); r.add_argument("images", nargs="+")
    r.set_defaults(fn=cmd_ref)

    g = sub.add_parser("ghost"); g.add_argument("notaa"); g.add_argument("taa")
    g.add_argument("--frame", type=int, required=True); g.set_defaults(fn=cmd_ghost)

    h = sub.add_parser("shift"); h.add_argument("a"); h.add_argument("b"); h.add_argument("depth")
    h.add_argument("--fov", type=float, default=60.0); h.set_defaults(fn=cmd_shift)

    args = ap.parse_args()
    return args.fn(args) or 0


if __name__ == "__main__":
    sys.exit(main())
