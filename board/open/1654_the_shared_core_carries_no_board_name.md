Type: bug
Area: render
Tags: hygiene

# The shared core carries no board name

src/render/stages/MediumCore.h:4-7 opens with a four-line block naming board:1580 — the only
site in src/ that spells a board item (grep -rn 'board:' src/ finds exactly this one). The
house rule is explicit: the code carries no commentary; work items live in board/, code never
names them. `git log --grep 'board:1580'` already carries the linkage; a number in the header
is the drift class the rule exists for (the item closes, the comment stays, the next reader
follows a stale pointer).

The dialect seam itself — the including side defines MEDIUM_CONST, MEDIUM_THREAD and
OUTSHINE_PI for its language before including — is a legitimate non-obvious why and may keep
ONE line saying that much. The board number, the file inventory of who includes it, and the
"scalar physics only" narration go; ParticipatingMedium.h:48-54 is the visible proof of the
contract either way.
