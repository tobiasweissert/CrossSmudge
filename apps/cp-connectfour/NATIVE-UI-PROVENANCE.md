The Sticky presentation follows CrossPlay commit
`d3a49ce8e85f9bd0d3e0c774a54adc267fd7547b`, including this title's original
Screens builders, Toybox geometry, marks, diagrams and Jersey font cuts.
`art/native-font-sources.json` pins the losslessly repacked original glyphs;
`art/nearby-source.json` identifies the original Nearby artwork. Existing
CrossPlay, Jersey OFL and Lucide license files accompany the assets.

The 480x800 normal menu, board samples, three lesson pages, all result variants,
last-completed-position ornament and long counters were compared against
actual original native pixels through production Lua callbacks. Landscape
boards/lessons/ornaments adapt fixed portrait geometry to keep controls inside
the screen; they do not claim full-frame original pixel identity.

Nearby reports that the current Lua host does not expose the network service.
Its notice retains the port's local two-player/new-solo actions. Play resumes
existing saved play; New Solo and Local Two-Player require confirmation before
replacing it. Play Again saves the completed game before starting another.

Only completions observed by this UI add to its app-local dual-slot checksum
record. Existing saves do not invent older history. Totals saturate at 65535;
invalid/overflowing counters are rejected. Record errors retain the game save
and show a retry notice. The game journal, core, AI and save formats are unchanged.

Non-Sticky devices and older hosts use byte-identical baseline legacy modules.
The original font assets remain present for that route. This is a local UI
change; manifest versions and store catalogues are unchanged.
