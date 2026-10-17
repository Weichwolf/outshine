# Original resident profile provenance (#660, work in progress)

The original32-source resident asset was prepared with an empty row ledger.
This correctly prevents module interior-profile binding, even where its original
recipe and actual owner caps still contain the needed information. Recover rows
from the identical hash-bound original recipe during preparation, preserve their
order and exact axes/widths, and bind their recipe/body hashes. Consumers read the
prepared metadata; they do not globally decode sources to reconstruct old profiles.

On5ee39968c a fresh actual53-source preparation succeeds. All six road NPZ hashes,
frame/source IDs, terrain, supports, catalog and display references are unchanged.
Only manifest commit and root row metadata differ. Manifest grows58464→129344B;
standalone recovered rows serialize to55313B.31of32axes are exactly original;
the intentionally altered32nd axis is preserved, not snapped back or admitted as
original. Existing actual-profile checks still reject changed/nonoriginal axes.
Nine source and nine installed controls pass, including wrong recipe and missing/
reordered sources. No native/library API changes.

The actual original32651280:3@8 root profile spans2m and passes existing complete
cap-line coverage, with three world Z values163.53526103045,163.52799539268017,
163.52799539268017. This is a varying physical profile, not a flat mean-height
shortcut. The unchanged78-source snapshot was used for the private metadata
hypothesis; no historical manifest or reader receipt was modified.

The full951.79m cycleway39881120:32 was then tried on the freshly prepared53-source
base. It passes profile lookup but remains invalid at the whole body interface:
old contact area776.114290m², new area1900.003866m², overlap area0 and distance0.
Their exact computed boundaries share only a point, not a complete line segment.
The existing nonempty whole-interface guard therefore refuses the module. Root
profile sampling alone cannot certify a connected whole body. No54/79-source
admission, tolerance relaxation or old-body snapping is claimed. The separately
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
