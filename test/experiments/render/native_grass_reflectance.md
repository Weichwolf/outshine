# Native coarse alpine grass reflectance — #451

The narrow product correction from #645/6ce858897 is present on the Metal
integration branch as24908bbb7, based on588e290f0. It restores the existing
alpine_grassland RGB mixture with provisional coefficient0.829. This is neither
an angular grass BSDF nor the forthcoming separate Grass/Dry material layer.
No shader, binding, geometry or per-frame loop changes.

## Actual Metal comparison

A18 Pro,1280×720/30, offline fresh processes,30 measured frames and360 degrees,
unchanged camera/time/world contents. Same matching built client and shaders;
only filesystem recipe changes. The complete recipe is already in the region
asset key. A miss rebuilds the finished region, then warm hits load it normally.
First changed preparation overlapped a nice10 module test; warm capture did not
have a second owned GPU consumer. No isolated performance ranking is inferred.

| Capture | PNG | Load ms | CPU p99 ms | GPU mean/max ms |
|---|---|---:|---:|---:|
| Original warm |df0510ba|859.900|7.418|20.684/36.986|
| Changed first asset build |076343f4|10604.170|3.207|17.033/41.518|
| Changed warm |076343f4|918.955|2.141|19.781/37.148|

All report22696 retained scene triangles and no bare terrain tiles; this field
is not total GPU terrain references. Changed first/warm PNGs are identical.
Both actual PNGs were opened: the central grey grassy slope becomes a muted
yellow-green while rock stays pale.214325/921600 pixels differ,45951 by more
than1/255, worst41/255. It remains a modest colour correction. Sparse near
crowns, dark leaf undersides, flat distant trees and ground distribution remain
major visible defects. Native plant integration is tracked under #1.

The two VegetationTemplates tests pass in3970ms. Product recipe and test are
from the checked upstream delivery; no C++ producer implementation changes.

## Budget remains open

All three GPU maxima exceed33.333ms. No device/frame acceptance or master
product merge follows from this comparison. First asset creation is separate
from the warm loading target. No threshold, profile or frame count is changed.

An additional instrumented capture attributes its36.597ms maximum to measured
frame1. Its terrain pass spans26.646ms; vertex26.592ms and fragment26.511ms
intervals overlap and must not be added or called proof of a vertex bottleneck.
Other measured frames range9.953–30.097ms. This identifies the expensive pass
and first-frame condition; the underlying cause is not yet established.

Logs, original PNGs, pixel comparison and complete instrumentation receipt:
`/tmp/outshine-grass-metal-588e290f0/`. The profiler is
`metal_work_profile.py`; this instrumented result is not a device gate.
