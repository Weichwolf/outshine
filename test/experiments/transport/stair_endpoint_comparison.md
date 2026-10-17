# Secondary original endpoint contacts

Owner: [#545](https://github.com/Weichwolf/outshine/issues/545), parent
[#434](https://github.com/Weichwolf/outshine/issues/434).

The earlier automatic adapter considered new endpoints at old interiors, but
missed an old endpoint meeting a new through source. The original provider node
`[2017,2560]` joins old `80922970:211@0` to new `80922970:280@1`. Explicit
`endpoint_contacts` retain both source IDs and both original vertex indices.
Their combined limit with interior contacts remains four. Actual old ground,
identical original axes/frame, unique nodes and complete sampled old profiles
are mandatory. A nearby overlap creates no connectivity or reuse permission.

The immutable old end need not cover the entire transverse profile of the new
through road. Its own complete profile still must agree with the new/reused
surface, as must the opposite contact walls. At a single new through source,
the additional transverse check compares positive-area facets on opposite
sides of its own profile, including explicitly borrowed old facets. Line-only
touches and sub-`1e-7`-m slivers do not prove a side; all trace intervals and
heights must agree in Double and Float32 with the existing derived guards.
Other junction checks remain. This corrects the previous end/through
specification; it does not enlarge its numerical or physical tolerances.

SIGGRAPH starting point: Chen et al., *Interactive Procedural Street Modeling*
(2008), page 5 figures 7–8 and section 6.1 inspected visually again. Keeping
original graph topology separate from geometric construction is useful here;
this is not their tensor-field generator. No SDK or foreign code is used.

`fixtures/stair-secondary-endpoint-vienna.json` retains two complete original
footways from the bounded five-source prefix proposal. The reproducible parent
is the four-module, 51-source saved specimen from #537. This is an alternative
53-source specimen, not a replacement for the earlier 53-source prefix specimen
from #539, and not a combined 55-source world. The public four-parent and
64-owner limits remain; the general local generator adapter from #542 is open.

## Validation

Source controls currently pass 128 tests, including seven endpoint/through
controls. Frozen full-product, installed-library and actual 1280×720 rendering
evidence is recorded after execution; these are not yet claimed by this text.
