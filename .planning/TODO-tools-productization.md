# TODO — tools productization (Kenny, 2026-08-28)

Started as QuestEditor notes; the scope is all 16 editors. Goal per Kenny:
"basic cleanup and make them usable, not perfection" — then package.

## 1. Port buildQuestCrcStringTables (QuestEditor)

The `Build Quest CRC Tables` button shells `perl buildQuestCrcStringTables.pl
--local <branch>` (ToolProcess.cpp:148-160), same missing-perl problem the
QuestChecker had. **Harder than QuestChecker: the .pl exists NOWHERE on this
machine** — not in the SOE tree (searched), not in the repo. Options, in order:
- hunt it in SWGSource/other repos' history (it's a build-pipeline script the
  server needs too — the community may carry it);
- else reimplement: it builds the quest CRC string tables the client/server
  use to map quest names to CRCs — the OUTPUT format is a known datatable /
  crc string table readable in engine source (`QuestManager` /
  `CrcStringTable` consumers), so a PowerShell reimplementation from the
  output format is feasible the way QuestChecker was.
Wire-up is the proven pattern: ToolProcess.cpp + a .ps1 in the tracked
`src/build/win32/exe/win32/` store.

## 2. Remove Perforce from all tools

Strip the P4 buttons/menus and the p4 spawn paths — they can only fail here
and some are actively dangerous because they AUTO-SAVE first. Known
touchpoints (each tool needs its own sweep):
- QuestEditor: ToolProcess::addToPerforce (6 files per save), p4 menu items.
- SwgDraftSchematicEditor: OnButtonP4edit **auto-saves before the p4 call**
  (SwgDraftSchematicEditorDoc.cpp:234-244).
- SwgSpaceQuestEditor: auto-save before its P4 op (Doc.cpp:504).
- SwgConversationEditor: auto-saves before Perforce/compile ops
  (Doc.cpp:1309, 1328); `p4Command` cfg key.
- ShipComponentEditor / TerrainEditor / others: check for p4 menu ids and
  `p4`/`perforce` strings per tool.
Also drop the Perforce client libs from the link where present.

## 3. Usability pass over every editor — "usable, not perfection"

A second sweep, tool by tool, fixing the known paper cuts. Seed list from the
save sweep (docs/TOOLS-GUIDE.md Part 2.5 has the full trap table):
- LightningEditor: the .swh writability-probe bug (0-byte litter + wrong-file
  probe) — MainWindow.cpp:255 one-liner.
- ClientEffectEditor: value edits in the tree revert; save silently no-ops on
  unmodified docs (add feedback).
- SwgConversationEditor: failed save returns TRUE and clears the modified
  flag (Doc.cpp:1118-1138) — make failure honest.
- NpcEditor: SaveDialog defaults point INTO the reference tree (clobbered two
  files once already); template-writer failures are silent stubs — surface
  them. Consider shipping replacement text/templates sources.
- QuestEditor: plain Ctrl+S writes back to the opened path with no prompt.
- TerrainEditor: Construction Layers tree cannot scroll (undiagnosed);
  null-boundary crash guard in place but cause unfound.
- SwooshEditor: animation demo inert (combat playback never engages).
- Everywhere: DEBUG_-only diagnostics that hide real failures in release
  (seven instances found so far) — promote the load-bearing ones.

## 4. Packaging / installer

Most SWG data ships in the SWGSource TREs — the tools additionally need the
LOOSE data that is NOT in any TRE. Plan:
- **Build the "not in TREs" inventory**: walk the SOE loose tree
  (`D:\SWG All Tools Working\swg\current`), for each file compute its
  TreeFile-relative name and check membership across the 209 TREs
  (scripts/trelist.py already indexes them). Output = the payload manifest.
- Known-required loose payload so far: 4,882 .lmg wearable meshes
  (appearance/mesh), the loose ui/ tree (170 files incl. ui_root_npceditor.ui
  — /AvView exists in NO TRE), ash/lat sources (203), .qst quests, the dsrc
  server+shared sources (QuestEditor/DraftSchematic/SpaceZone/SpaceQuest all
  need them), QuestEditorConfig.xml, NpcEditor.tab/SwooshEditor.tab, the
  spell-check .dct files, loose .prt/.snd/.swh/.ltn/.cef samples for content
  work.
- **Installer**: package that payload + the tool exes + the ported .ps1
  scripts + DataTableTool.exe; installer writes the install root into the
  tool cfgs (the cfgs are tracked and machine-specific today — parameterize
  the three roots: TRE dir, loose-data dir, output dir). The relative
  searchPath trick (TreeFile_SearchNode resolves against CWD) is the
  portable alternative to absolute paths where layout allows.
- Ship legacy no-sku TreeFile keys in tool cfgs (never client.cfg's _00_
  form) — see the memory note; this stays the #1 config landmine.

## 5. Survey the command-line tools — restore the end-to-end toolkit

The editors shell out to a whole CLI layer; inventory which ones the pipeline
actually needs and bring each up the way DataTableTool was (in-tree source,
x64 build, parser fix already inherited via sharedFoundation). Known so far:
- **DataTableTool** — DONE (built x64, works, .tab -> .iff).
- **templatecompiler** — SwgDraftSchematicEditor's Compile shells
  `templatecompiler -compileeditor` for both .tpfs (Doc.cpp:223-229); the
  server also needs it for all object templates. In-tree source? Check.
- **miff** — .mif -> .cdf/.iff compiler (NpcEditor writes .mif sources; the
  client reads compiled .cdf). SOE exe exists; in-tree source likely
  (sharedFile tools).
- **TreeFileBuilder / TreeFileExtractor** — TRE pack/unpack; needed for
  shipping content back into TREs. SOE exes exist in the reference tree.
- **ViewIff** — diagnostic, nice-to-have.
- The mocha conversation compiler (mochac.pl) — perl again, script ABSENT
  locally; SwgConversationEditor's Compile depends on it. Same
  find-or-reimplement decision as buildQuestCrcStringTables.
Audit method: grep the editors for ProcessSpawner/QProcess/CONSOLE_EXECUTE
spawns to get the complete list of external commands, then triage.

## 6. Endgame: replace the CLI layer with SWG-Toolkit

Long-term direction: the legacy CLI tools are stopgaps; the durable home for
format tooling is **SWG-Toolkit** (D:/Code/SWG-Toolkit — already the format
REFERENCE for TRE per the standing rule; tre-lint at
D:/Code/swg-client-v2/tools/tre-lint has the verified v0005/v0006 layouts).
Per-format migration: TRE pack/unpack, datatable tab<->iff, IFF inspect
(iffcensus.py is a seed), template compile, string tables. Editors would then
shell the toolkit's CLIs instead of the 2004 exes — the ToolProcess/
CONSOLE_EXECUTE seams found in item 5 are exactly where they swap in.
