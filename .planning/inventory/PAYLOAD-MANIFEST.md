# Not-in-TREs inventory — installer payload manifest (TODO item 4)

Generated 2026-08-30 by `scripts/not_in_tres_inventory.py`. Regenerate any time
(~1 min):

```
python scripts/not_in_tres_inventory.py
```

Inputs: the 209 SWGSource v3.0 TREs (`D:\Code\SWGSource Client v3.0`, 217,533
entries, 150,575 unique names — trelist.py now reads BOTH v0005 and v0006, so
this census is exhaustive, unlike the pre-guard sweeps) vs the SOE loose tree
(`D:\SWG All Tools Working\swg\current`). Full per-file results:
`data-manifest.csv` (295k rows) + `dsrc-manifest.csv` (85k rows) in this
directory — NOT committed (26 MB + 8 MB, regenerable); `summary.txt` is.

## Headline numbers

| payload class | files | size |
|---|---|---|
| A. data files in NO TRE | 121,024 | 2,617 MB |
| A. dsrc sources (never in TREs) | 85,431 | 258 MB |
| B. cfg-referenced loose dirs (overlaps A) | see below | — |
| exe/win32 config payload | ~60 small files | < 5 MB |

Per data root (files / total MB / **not-in-TRE** files / not-in MB / shadow):

```
sku.0/sys.client/compiled/game   209,139  7,421 MB   60,469  2,470 MB   7,330
sku.0/sys.server/compiled/game    48,656    143 MB   48,627    123 MB      17
sku.0/sys.shared/compiled/game    36,894     72 MB   11,928     25 MB   4,756
(the three .../built/game roots hold 1 file each, all TRE-covered)
```

Not-in-TRE data by top-level dir, largest first: texture 1,042 MB (8,884 .dds),
appearance 685 MB (23,984), sample 302 MB, video 162 MB, music 90 MB,
player_music 69 MB, object 62 MB (47k tiny .iff), datatables 59 MB, script
50 MB (5,658 .class — the whole server script tree), voice 29 MB, quest 27 MB
(1,300 .qst — quest content is 100% loose, zero .qst in any TRE), terrain
22 MB, then a long tail (ui 4.4 MB incl. ui_root_npceditor.ui, clienteffect
176 files, etc.).

## Two payload classes — the distinction that matters

**Class A — files in no TRE.** TreeFile can never serve them; without a loose
copy the data does not exist. The `in_tre == 0` rows of data-manifest.csv.

**Class B — directories the tools open with the FILESYSTEM, not TreeFile.**
The cfg keys point editors straight at loose dirs (browse defaults, source
dirs, WRITE targets). These must exist loose **even where every file is also
in a TRE**. From the cfg cross-reference (all `SWG All Tools Working`
references in the Release + exe/win32 cfgs):

| loose dir (under the matching root) | files | MB | cfg keys / tools |
|---|---|---|---|
| client `appearance/mesh` — *.lmg only | 4,882 | 1.1 | NpcEditor wearableDirectory |
| client `quest/` | 1,671 | 32 | QuestEditor defaultQuestDirectory |
| client `string/en/` | 5,494 | 46 | SpaceQuest sharedStringFilePath, ConvEditor stringPath (WRITE target) — 0 not-in-TRE, ships for browse/write |
| client `ui/` | 203 | 16 | UIBuilder searchpaths; ui_root_npceditor.ui (NpcEditor /AvView) |
| client `clienteffect/` | 1,048 | 0.1 | CEE scoped mount (stage-cee-loose junction) |
| server `object/` | 30,086 | 37 | NpcEditor weapons, QuestEditor serverObjectTemplateDirectory, GodClient |
| server `datatables/` | 9,557 | 35 | SpaceQuest/SpaceZone/GodClient |
| server `script/` | 5,658 | 50 | GodClient scriptClassPath |
| server `misc/` | 14 | 23 | object_template_crc_string_table.iff (ShipComponent, GodClient) |
| shared `object/` | 30,006 | 48 | GodClient template paths |
| shared `datatables/` | 6,530 | 24 | QuestEditor questlist (CRC script walks it) |
| `dsrc/` (all three sys.*) | 85,429 | 258 | NpcEditor/QuestEditor/DraftSchematic/ConvEditor/SpaceQuest/GodClient sources — READ AND WRITE |

Note the .lmg correction: the TODO said "4,882 .lmg wearable meshes" as if
not-in-TRE; actually only 1,819 are absent from TREs. Irrelevant for packaging
— NpcEditor enumerates the directory, so all 4,882 ship (1.1 MB, trivial).

## THE SHADOW TRAP — why the installer must use scoped mounts

**12,106 loose files share a TRE name but match no TRE copy's size** (object
4,608, datatables 2,528, appearance 2,160, string 855, ...). The SOE tree is
2016 dev data; the TREs are v3.0. Mounting a broad loose root over the TREs
makes these shadow files WIN and changes engine behavior — this is exactly the
walking-avatar bug (08-29): a whole-tree searchPath11 mount made the CEE
avatar walk. And priority does NOT fix it: TreeFile registers searchPaths
ahead of searchTrees at each priority (TreeFile.cpp:345), so a loose root
shadows TREs even at priority 0.

Rule for the installer: **loose dirs are for filesystem access (Class B) and
for scoped TreeFile mounts only** (junction/subdir per data type, like
stage-cee-loose). Never mount a whole `compiled/game` root as a searchPath.
(QuestEditor.cfg's searchPath10/11 currently DO mount whole roots — works,
but carries the shadow risk; candidate for scoping during cfg
parameterization.)

## exe/win32 payload (beside the tool exes)

Tracked already in `src/build/win32/exe/win32/`: QuestEditorConfig.xml,
SwgDraftSchematicEditor.cfg/.ini, SwgConversationEditor.ini, NpcEditor.tab,
SwooshEditor.tab, QuestChecker + BuildQuestCrcStringTables .ps1. From the SOE
exe/win32 still referenced by cfgs and NOT tracked: the two spell-check
dictionaries `SwgConversationEditor_medium.dct` / `_user.dct` (ConvEditor
dictionary keys), `godclient_favorites.xml` (copied to Release). mochac.pl
(mochaCommand key) exists NOWHERE — key is now unread after the p4 strip,
dialog was fixed 08-28.

Out-of-repo pieces the installer must also carry (lost on wipe today):
- `D:\Code\Galaxies-Reborn\stage-B-override\ui\ui_root_npceditor.ui`
  (searchPath12 override; NpcEditor dies without it)
- `D:\Code\Galaxies-Reborn\stage-cee-loose\clienteffect` junction (recreate:
  `New-Item -ItemType Junction -Path ...\stage-cee-loose\clienteffect -Target
  "D:\SWG All Tools Working\swg\current\data\sku.0\sys.client\compiled\game\clienteffect"`)
- compiled_shader cache in Release (regenerable, avoids first-run stalls)

## Trimming opportunities (payload could drop well under 1 GB)

The 2.6 GB Class A is the *superset*. Big chunks are plausibly not needed by
any tool: `sample/` 302 MB (audio samples — SoundEditor might browse them),
`video/` 162 MB, `music/` + `player_music/` 159 MB, `voice/` 29 MB. texture/
1,042 MB not-in-TRE is the big question mark — how much is referenced by
not-in-TRE appearances vs dead dev data is unknown. A reference-closure pass
(walk .apt/.sat/.msh/.sht chains) would answer it; not done. Decision for
later: ship-everything (~3 GB) vs closure-trimmed.

## Suggested cfg parameterization (three roots)

Every machine-specific path in the cfgs is built from three roots:
1. TRE dir (`D:\Code\SWGSource Client v3.0`)
2. loose-data root (`D:\SWG All Tools Working\swg\current`) — data/ + dsrc/
3. output/scratch root (C:\save-test today)

The installer writes these into the cfgs (or a shared include cfg). Keep the
legacy no-sku TreeFile keys — never client.cfg's `_00_` form (standing #1
landmine).
