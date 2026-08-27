Type: bug
State: open
Parent: 1953
Area: engine

# `Live` keeps its own counters private

The corrected access audit (board:1972) shows four public data members with PRIVATE NAMES sitting
in `class Live`'s public section:

    static size_t TookPosing_, TookSubmitting_, TookAiming_, TookDrawing_;
    static size_t AssetReads_;
    static size_t PlanInits_;
    Render::PlanSpec PlanDeclared_;

Each already has a public accessor two lines below it (`TookPosing()`, `AssetReads()`,
`PlanInits()`), so the data is public for no reason at all -- the door beside it is the one the
tree uses. The trailing underscore says what was meant; the access says the opposite, and where a
name and a keyword disagree the keyword wins.

- [ ] the four are private and the accessors stay
- [ ] `PlanDeclared_` is reached through a verb or it is not reached
- [ ] the access audit's declared count falls by four
