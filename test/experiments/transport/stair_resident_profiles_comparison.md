# Original resident profile provenance (#660, work in progress)

The original32-source resident asset was prepared with an empty row ledger.
This correctly prevents module interior-profile binding, even where its original
recipe and actual owner caps still contain the needed information. Recover rows
from the identical hash-bound original recipe during preparation, preserve their
order and exact axes/widths, and bind their recipe/body hashes. Consumers read the
prepared metadata; they do not globally decode sources to reconstruct old profiles.

On5ee39968c a fresh actual53-source preparation succeeds. All six road NPZ hashes,
frame/source IDs, terrain, supports, catalog and display references are unchanged.
Only manifest commit and root row metadata differ. The initial full-row attempt
grew58464→129344B. Final preparation98371a240 retains only source ID, actual
axis, width and provenance:95545B, saving33799Bagainst that attempt while all
six body hashes remain identical. Unused gradients/DEM priors/recipe details are
not duplicated in the prepared row ledger. The source decoder still reconstructs
those rows during preparation; no decode-time saving is claimed.31of32axes are exactly original;
the intentionally altered32nd axis is preserved, not snapped back or admitted as
original. Existing actual-profile checks still reject changed/nonoriginal axes.
109source and109installed controls pass, including the actual200-face original
cap fixture and wrong axes/coordinate frames/widths/owners, missing intervals,
wrong recipe and missing/reordered sources. No native/library API changes.

The actual original32651280:3@8 root profile spans2m and passes existing complete
cap-line coverage, with three world Z values163.53526103045,163.52799539268017,
163.52799539268017. This is a varying physical profile, not a flat mean-height
shortcut. The unchanged78-source snapshot was used for the private metadata
hypothesis; no historical manifest or reader receipt was modified.

The full951.79m cycleway39881120:32 was then tried on the freshly prepared53-source
base. It passes profile lookup but remains invalid at the whole body interface:
old contact area776.114290m², new area1900.003866m², overlap area0 and distance0.
Their exact computed boundaries share only a point, not a complete line segment.
The existing nonempty whole-interface guard therefore refuses that level_box
alternative. Exact rational determinants confirm tiny nonzero endpoint offsets
from old lines; measured point/edge distances are about1.11e-14and2.72e-15m.
No general GEOS error bound or repair follows from this observation.

The existing shared_box alternative passes a whole1.900073m interface with
unchanged contact guards and actual oppositely oriented walls. Body/profile and
headroom checks also pass before a genuine physical rejection: pier123 intersects
the new cycleway near original axis station602.785m, between knots19/20. Both
Double/Float32distance are zero, below the unchanged0.2mrequirement. The pier
bounds are[145.6191,705.3008]–[146.5342,706.1199]. This requires a distinct
physical source-adaptation plan, not a precision excuse. No54/79-source admission,
tolerance relaxation, old-body snapping or new clipping algorithm is claimed. The separately
verified78-source delivery from #661 remains intact.

One metadata recovery after imports took1.854367s with11,235,138B Python-traced
peak; process peak RSS126,717,952B includes imports. This includes actual recipe
loading, source decode and sampling, excludes serialization. Native allocations
are incompletely represented; one loaded WSL2/i5-1135G7/nice10/tracemalloc run is
not a device or uninstrumented timing claim. The one-time preparation cost and
additional metadata bytes must be justified by avoiding later reconstruction.

Actual inputs, new preparation, byte comparisons, positive cap profile and
negative whole-interface WKB/full body arrays remain under
`/tmp/outshine-transport-resident-8f5404b41/`. Shared historical inputs/receipts
remain protected. Open work: explain and repair the actual new-body/profile
intersection, negative owner/frame/width cases, independent complete module/world
readers, whole720pimages and equal-result costs before #660 can close.


SIGGRAPH starting point: Cherchi et al., [Interactive and Robust Mesh Booleans,
SIGGRAPH Asia2022](../../../doc/references/vegetation/siggraph/2022-interactive-robust-mesh-booleans.pdf),
page2/Figures2–3 actually opened. The paper separates surface intersection and
classification and explains finite-precision failures. It supports investigating
the complete geometry rather than treating exact boundary predicate success as
physical truth. It does not establish our metric bounds, profile result or algorithm.
The already implemented shared_box alternative resolves this particular interface;
no foreign code or new robust-Boolean implementation is adopted.
