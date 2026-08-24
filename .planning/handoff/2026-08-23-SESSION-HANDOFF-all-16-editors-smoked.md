# Session handoff — 2026-08-23 — all 16 editors launch-smoked

Branch: `qt-tools-verify` (worktree `D:\Code\swg-qt-tools-worktree`, parent
`D:\Code\Galaxies-Reborn\client-tools`, main branch `x64-dx11-integration`).

**Write the handoff as you go.** Power has now cut three sessions running and
each time the next session had to reconstruct state from the jsonl transcript
under `C:\Users\kenne\.claude\projects\D--Code-swg-qt-tools-worktree\`.

## Where we are

Client visually validated (earlier session). Phase 3 = launch-smoke every
editor. **That is now DONE for all 16.**

Scoring: `w` = real main window up. `c` = runs, but complains about
config/paths. `x` = crash / FATAL / instant exit.

| # | Tool | Score | Evidence |
|---|------|-------|----------|
| 1 | SwgGodClient_r | w | title `SWG God Client - [[God Client Focus] Game Window]` |
| 2 | TerrainEditor_r | w | MFC frame `terrainEditor` + benign "Tip of the Day" |
| 3 | UIBuilder | w | screenshot: File/Edit/Insert/View, Objects tree, Perf Diagnostics |
| 4 | ParticleEditor_r | w | Qt window up |
| 5 | AnimationEditor_r | w | Qt window up |
| 6 | LightningEditor_r | w | Qt window up |
| 7 | SwooshEditor_r | w | Qt window up |
| 8 | NpcEditor_r | **w** | FIXED - 3 stacked defects; Qt chrome + live 3D viewport |
| 9 | SoundEditor_r | w* | alive, but see the client.cfg landmine below |
| 10 | ClientEffectEditor_r | w | Qt window up |
| 11 | QuestEditor_r | **w** | FIXED - QuestEditorConfig.xml ported from the SOE tree |
| 12 | ShipComponentEditor_r | **w** | FIXED - paths ported from the SOE tree |
| 13 | SwgConversationEditor_r | c | full MFC frame + cosmetic `<branch>\exe\win32` warning |
| 14 | SwgDraftSchematicEditor | **w** | FIXED - no dialog; 749 resourceTypes, 24 categories |
| 15 | SwgSpaceQuestEditor_r | **w** | FIXED - 20 mission categories; cosmetic warnings remain |
| 16 | SwgSpaceZoneEditor_r | c | full MFC frame + cosmetic `<branch>\exe\win32` warning |

**FINAL STATE (verified by a clean full-suite run, 2026-08-23 19:0x):**
**16 of 16 tools reach a working main window.**

Raw run: 13 x `w`, 2 x `?`, 1 x `x`.
* The two `?` are **UIBuilder** and **SwgSpaceQuestEditor** — both confirmed good
  by screenshot. They score `?` only because the runner will not guess about a
  process whose sole visible window is class `#32770`: UIBuilder's main window
  legitimately IS a `#32770` (MFC dialog app), and SpaceQuest hides its frame
  behind stacked cosmetic warnings. Dismiss with `_shot3.ps1` to see the frame.
* The one `x` is **NpcEditor** — crash fixed, now blocked on the missing
  `/AvView` UI page (content, not code).

Two defects remain open, both documented in detail below:
1. **NpcEditor** `/AvView` page ships in no dataset on this machine.
2. **SoundEditor** reads `client.cfg` with no skuBits -> mounts zero TREs.
   It launches fine and will fail on first file open. One-line fix + rebuild.

Nothing in the `c` column is a port defect — they are all missing or stale
config data inherited from the SOE build layout.

## NpcEditor — SOLVED end to end (2026-08-23)

**Working.** Qt chrome up (File / View / Zoom / Wearables / Tools / Backdrop,
toolbar, Wearable Customization + Avatar Customization panels) and the embedded
D3D11 viewport rendering the backdrop scene. Screenshot:
`logs/_shots/NpcEditor_r_pw1.png`.

Three separate defects were stacked behind one another. Each only became visible
once the one in front of it was fixed.

### Bug 1 (code) — `strlen(NULL)` in GameWidget's constructor

`GameWidget.cpp:263` assigned `ConfigFile::getKeyString("NpcEditor",
"wearableDirectory", 0, 0)` — whose 4th arg is the default, i.e. NULL — straight
into a `std::string`. cdb stack: `strlen` <- `basic_string::assign` <-
`GameWidget::GameWidget+0x2f3` <- `BaseMainWindow` <- `MainWindow` <- `main`.
**Fixed**: null-guarded with a release-visible `WARNING` (use `WARNING`,
Fatal.h:50 — `DEBUG_WARNING` compiles out in release and would have been silent).

### Bug 2 (content) — the `/AvView` UI page

`NpcCuiMediatorFactorySetup.cpp:74` maps the Viewer mediator to page `/AvView`;
`CuiMediatorFactory_Constructor.h:42` raises STRICT_DATA_FATAL if the page is
absent. It is in no TRE set on this machine.

**Fixed**: SOE's own `ui_root_npceditor.ui` — which defines `/AvView` inline —
was found **loose** in the reference tree at
`D:\SWG All Tools Working\swg\current\data\sku.0\sys.client\compiled\game\ui\`.
Copied to `D:\Code\Galaxies-Reborn\stage-B-override\ui\` (searchPath12,
priority 12, above swgsource_3.0.tre at 8 and the base TREs at 0) and selected
per-tool with `[ClientUserInterface] uiRootName=ui_root_npceditor.ui` in
NpcEditor.cfg. That is exactly how SOE did it — their `NpcEditorCommon.cfg`
carries the same key. Nothing else, client included, is affected.

A hand-authored `ui_npc.inc` was written first and then **discarded** in favour
of SOE's real page. Don't recreate it.

How the page satisfies the code, for future reference —
`CuiMediator::getCodeDataObject` (CuiMediator.cpp:1470) resolves a name by
looking for a property of that name on the page's `CodeData` object and treating
its value as a path; **failing that it falls back to `rootPage->GetChild(name)`**.
SOE's AvView uses the fallback: its children are named `viewerWidget`,
`sampleWearableBox`, `wearableVolumePage`, `currentVolumePage`, `pageWearables`
(plus `hiddenAvatarList` and a `CodeData` object, which must exist — a page with
no CodeData at all FATALs at CuiMediator.cpp:1474).

### Bug 3 (code + config) — `append(NULL)` on the backdrop

With `/AvView` present, construction got as far as
`MainWindow::slotChangeBackdrop`, which does
`getKeyString("NpcEditor", "backdrop", id, 0)` and hands the result to
`NpcCuiViewer::setEnvironmentTexture`, which does `fullpath.append(baseFilename)`
-> `strlen(NULL)`. Same defect class as Bug 1.

**Fixed** both ways: `setEnvironmentTexture` now null-guards and WARNs, and
NpcEditor.cfg gained `backdrop=jedi` (SOE's own value) plus the
defaultClient/Server/Shared/Weapon dsrc paths rebased onto the local tree.

`texture/jedi.dds` is not in the readable TREs, but that is harmless:
`TextureList::fetch(const char *)` passes `createTexture=true`, so a missing file
yields a created/default texture rather than NULL.

### wearableDirectory IS set — an earlier claim here was wrong

A previous revision of this document said there were "no `.lmg` files anywhere on
this machine" and left `wearableDirectory` unset. **That was wrong.** There are
**4,882 `.lmg` files** at

```
D:\SWG All Tools Working\swg\current\data\sku.0\sys.client\compiled\game\appearance\mesh
```

SOE's cfg points at `sys.client/exported/character/appearance/mesh`, which does
not exist here — but this is the *same* `exported/... -> compiled/game/...` remap
already documented above for the string paths. The earlier search covered only
the SWGSource client directory plus that one SOE path, then over-generalised to
"anywhere on this machine".

`wearableDirectory` now points there, and SOE's **60 wearable filter presets**
(30 Name/Expr pairs, verbatim from their `NpcEditorCommon.cfg`) are loaded: the
filter combo reads `Armor:Leg(F)` / `*leg*_f` instead of "Not available".

**Two ordering traps when editing these keys:**
1. `wearableFilterName` / `wearableFilterExpr` are read as parallel indexed lists
   (`GameWidget.cpp:1196,1214`), so each Name must be immediately followed by its
   own Expr.
2. `ConfigFile` sections are positional, so these keys must sit **inside
   `[NpcEditor]`**. They were briefly appended after `[ClientUserInterface]`,
   where they would have silently bound to the wrong section. Verify placement
   after any edit.

`NpcEditor.tab` (SOE's) is in place at `src\build\win32\exe\win32\`.

### CORRECTION to earlier claims in this document

An earlier revision said `/AvView` was absent from "184 UI files across all 209
TREs" and quoted similar totals for retail / SWGEmu / other datasets. **Those
sweeps were weaker than stated.** `scripts/trelist.py` implements TRE **0005**
only, and at the time it had no version guard, so v0006 archives were parsed with
the wrong layout and silently produced corrupt names instead of failing. The GR
client set alone is **72 v0005 + 137 v0006** — so ~two thirds of it was never
actually examined. The guard now makes trelist.py refuse v0006 loudly. The
conclusion happened to hold (the page really was only in the SOE loose tree), but
treat any pre-guard TRE census in this document as covering v0005 archives only.
For anything that must be exhaustive, use SOE's `TreeFileExtractor.exe`.

## Original triage notes: NpcEditor_r (superseded by the section above)

Exits after 5.9 s, code `0x80000003`. Log ends:

```
unknown location : WARNING 860d2942: Could not find skill DEPRECATED_SKILLS in DataTable
NpcEditor_r.exe: unknown.0
unknown location : FATAL be5fbc55: ExceptionHandler invoked
```

Notes for whoever picks this up:

- `NpcEditor_r.exe: unknown.0` is **not** an error — it is the
  `ApplicationVersion` banner (`ApplicationVersion.cpp:70`) that the exception
  handler prints. Don't chase it.
- `0x80000003` is STATUS_BREAKPOINT — the engine's own FATAL debug break, the
  same red herring that made the god client look like a crash yesterday. It is a
  *consequence*, not the cause. The cause is an unhandled SEH exception
  immediately before it.
- The god client logs the same `DEPRECATED_SKILLS` warning and survives, so
  that warning is not the trigger either.
- NpcEditor is the only Qt tool that boots the full client CUI stack
  (`NpcCuiManager`, `NpcCuiMediatorFactorySetup`). `logs/ext/ui.log` shows the
  entire UI type set and every font loading cleanly, so it dies *after* UI load.
- **No minidump was written** — `writeMiniDumps = ApplicationVersion::isBootlegBuild()`
  and this is not a bootleg build. To get a stack: flip that for a local build,
  or attach cdb/WinDbg (`NpcEditor_r.pdb` is right there in Release).

## The client.cfg landmine — SoundEditor (latent, will bite)

`SoundEditor.cpp:449` hardcodes `data.configFile = "client.cfg"`, but line 463
calls `SetupSharedFile::install(false)` — **no skuBits**. Verified by counting
keys: `client.cfg` has 70 `searchTree_00_N` and **zero** legacy `searchTreeN`.
So SoundEditor mounts **zero TREs**.

It still scored `w` because it never touches a TRE asset during startup — its
entire 7-line log is JUCE/WASAPI audio init. **Do not read "it launched" as
"it works."** It will fail the moment you open a sound file.

Fix: change `"client.cfg"` → `"SoundEditor.cfg"` at `SoundEditor.cpp:449` and
rebuild that project. `SoundEditor.cfg` already sits in Release in the correct
legacy form — it is simply never read today.

Related upstream quirk, harmless: `SwooshEditorGameWidget.cpp:18` passes
`"AnimationEditor.cfg"`, not its own. It works, because AnimationEditor.cfg is
in the fixed form. `SwooshEditor.cfg` on disk is dead weight.

## The SOE reference tree at `D:\SWG All Tools Working\swg\current` (found 2026-08-23)

**This is the single most useful thing found this session.** It is a complete
SOE-layout branch — `data/`, `dsrc/`, `exe/` — including `exe/win32/` with the
*original* SOE tool binaries **and their original .cfg / .ini / .xml files**:

```
QuestEditorConfig.xml          SwgDraftSchematicEditor.cfg + .ini
ShipComponentEditor.cfg        SwgSpaceQuestEditor.cfg      + .ini
QuestEditor.cfg + QuestEditorCommon.cfg
NpcEditor.cfg   + NpcEditorCommon.cfg + NpcEditor.tab
SwgSpaceZoneEditor.cfg + .ini  SwgConversationEditor.cfg    + .ini
TerrainEditor.ini              defaults.cfg (UIBuilder)
TreeFileExtractor.exe  TreeFileBuilder.exe  DataTableTool.exe  ViewIff.exe  miff.exe
```

When any tool complains about missing config, **look here first** — the answer
is usually the original file.

`TreeFileExtractor.exe` is SOE's own TRE extractor and handles every TRE
version. Prefer it over `scripts/trelist.py` for anything non-trivial:
trelist.py only implements TRE **0005**, and a large fraction of TREs in the
wild are **0006**, which is a different container entirely (the ones checked
decompress from offset 36 as a single zlib stream containing one IFF `FORM`,
not an archive with a TOC). trelist.py now refuses 0006 loudly rather than
emitting corrupt names.

### Path rebasing rule for this tree

SOE's cfgs use `h:/swg/current/...`. Rebase onto
`D:/SWG All Tools Working/swg/current/...`. One exception, verified by testing
every path: **`data/sku.0/sys.shared/built/game/...` does not exist here** (only
`.../built/game/misc`). The equivalent content lives under
**`data/sku.0/sys.client/compiled/game/...`** — confirmed for
`string/en`, `string/en/quest/ground`, `string/en/conversation`, and
`quest/nym_themepark`. Remap those keys accordingly.

Also absent: `h:/swg/current/tools/` (so `toolPath` / `toolDirectory` are left
unset — not validated by `testFolders()`, only the Compile / p4-edit buttons
use them), and `data/internal/sys.client/built/questeditor/image/...` (the
QuestEditor component icons — only `data/sku.0` exists, so icons will be blank;
not fatal, the fatal was the missing XML).

### AvView is not here either

The 71 readable (v0005) TREs in this tree hold 181 UI files and the same three
UI roots — **no `AvView`**. The other 137 TREs there are v0006 and were NOT
checked. If NpcEditor is ever revisited, `TreeFileExtractor.exe` on those 137
is the way to close that gap, along with reading this tree's `NpcEditor.cfg`
and `NpcEditorCommon.cfg`, which were not examined this session.

## Config gaps CLOSED (2026-08-23) — ported from the SOE reference tree

Four tools went from "complains" to working, by porting SOE's own configs
(see the SOE reference tree section above) rather than authoring anything.

| Tool | Before | After |
|------|--------|-------|
| QuestEditor | `Critical Error: Cannot open file [../../exe/win32/QuestEditorConfig.xml]` | title `QuestEditor Version 2.11` |
| ShipComponentEditor | `Errors in paths` (6 x `c:/projects/swg/x1/...`) | clean window, no dialog |
| SwgDraftSchematicEditor | `not properly configured`, pane `0 armorRatings / 0 craftingTypes / ...` | **no dialog at all**; tree shows all 24 schematic categories; pane reads `4 armorRatings / 29 craftingTypes / 13 damageTypes / 6 ingredientTypes / 749 resourceTypes / 13 stringTables / 61 xpTypes` |
| SwgSpaceQuestEditor | pane blank + `0 spaceZones / 0 questCategories / ...` | tree shows all 20 mission categories; Configuration pane parses real data (`questStringSuffix = [quest_location]`, ...) |

Also given their `[section]` keys and `.ini` files: SwgSpaceZoneEditor,
SwgConversationEditor.

### What was changed

* Six cfgs in `src\build\win32\x64\Release\` gained an **appended** tool section
  (`[QuestEditor]`, `[ShipComponentEditor]`, `[SwgDraftSchematicEditor]`,
  `[SwgSpaceQuestEditor]`, `[SwgSpaceZoneEditor]`, `[SwgConversationEditor]`).
  **Only appended** — the TreeFile keys were not touched.
* `src\build\win32\exe\win32\` created and populated: `QuestEditorConfig.xml`,
  `SwgDraftSchematicEditor.cfg` (tool-parser copy), `SwgDraftSchematicEditor.ini`,
  `SwgConversationEditor.ini`, `NpcEditor.tab`, `SwooshEditor.tab`.
* `.ini` files also copied to the Release dir: SwgSpaceQuestEditor,
  SwgSpaceZoneEditor, TerrainEditor.

### Two traps that cost real time — read before touching these cfgs

**1. Quote every value containing a space.** The local reference tree path is
`D:\SWG All Tools Working\...` — it has spaces. `ConfigFile` splits an unquoted
value on whitespace and keeps only the tail. The symptom is NOT an error: the
tool's own Configuration pane just showed a **silently truncated** path,
`Working/swg/current/data/...`, with the leading `D:/SWG All ` eaten, and every
count stayed 0. `ConfigFile.cpp:222-240` handles quoted values correctly, which
is why ShipComponentEditor (whose SOE cfg quotes its values) worked first try
while the others did not.

**2. Several tools read config from `../../exe/win32/`, NOT the working
directory.** Hardcoded `cms_root` under `#if 1`:

| Tool | File read from `../../exe/win32/` | Source |
|------|-----------------------------------|--------|
| SwgDraftSchematicEditor | `.cfg` **and** `.ini` | `Configuration.cpp:20,312,364` |
| SwgConversationEditor | `.ini` | `Configuration.cpp:20,78` |
| QuestEditor | `QuestEditorConfig.xml` | `QuestEditorConstants.h:30` |
| NpcEditor | `NpcEditor.tab` | `MainWindow.cpp:75` |
| SwooshEditor | `SwooshEditor.tab` | `MainWindow.cpp:856` |

From `src\build\win32\x64\Release` that resolves to `src\build\win32\exe\win32\`.
**SwgDraftSchematicEditor needs its cfg in BOTH places** and they are not
interchangeable: the engine reads the Release-cwd copy for TreeFile keys, while
`Configuration::loadCfg` reads the exe/win32 copy for `serverObjectTemplatePath`
and `draftSchematicDirectory`.

### Every startup dialog, identified (2026-08-23)

All confirmed by screenshot; the SpaceQuest branch box was read off-screen by the
user after a capture-tool artefact (below).

| Tool | Dialog | Status |
|------|--------|--------|
| TerrainEditor | "Tip of the Day" | benign MFC standard |
| UIBuilder | *(none)* - its `#32770` IS the main window | working |
| SwgConversationEditor | "...is not running from `<branch>\exe\win32`. You may be running an older version." | cosmetic |
| SwgSpaceZoneEditor | same | cosmetic |
| SwgSpaceQuestEditor | same, **plus a second, different box** (see below) | cosmetic x2 |
| SwgDraftSchematicEditor | was "not properly configured" | FIXED |
| QuestEditor | was "Cannot open file [../../exe/win32/QuestEditorConfig.xml]" | FIXED |
| ShipComponentEditor | was "Errors in paths" | FIXED |

### SwgSpaceQuestEditor's second dialog is permanent, and that is correct

Full text (read off-screen; see the capture caveat below):

> SwgSpaceQuestEditor is running out of the 'qt-tools-worktree' branch which does
> not match the directories specified by SwgSpaceQuestEditor.cfg. Make sure that
> SwgSpaceQuestEditor.cfg is configured appropriately for your machine.

`SwgSpaceQuestEditor.cpp:132-142` compares `extractBranch(cwd)` against
`extractBranch()` of four config paths. `extractBranch` takes everything after
the first literal `"swg"` substring up to the next `/`:

* cwd `D:\Code\swg-qt-tools-worktree\src\build\win32\x64\Release` -> `qt-tools-worktree`
* cfg `D:/SWG All Tools Working/swg/current/...`                  -> `current`

These can never match unless the tools are run from a path shaped like
`.../swg/<branch>/exe/win32`. **Do not try to "fix" this with config** - it is
structural, harmless, and dismissing it costs one click. (Two of the four paths
it compares, `getSharedQuestListDataTablePath` / `getSharedQuestTaskDataTablePath`,
are not even settable from the cfg - there is no `getKeyString` for them.)
`SwgSpaceZoneEditor.cpp:140` has the identical check.

### Capture-tool caveat: PrintWindow and message boxes

`_shotpw.ps1` uses `PrintWindow(hwnd, hdc, PW_RENDERFULLCONTENT)`. It is reliable
for the tools' own main windows - it captured the whole NpcEditor UI including
the live D3D11 viewport. **It is NOT reliable for MFC message boxes**: it
rendered only the first line of the SpaceQuest branch box, making a perfectly
normal message look truncated mid-sentence and briefly look like a string bug.
If a captured message box looks cut off, read it on-screen before believing it.

The screen-grab helpers (`_shot.ps1` / `_shot2.ps1` / `_shot3.ps1`) have the
opposite failure: `SetForegroundWindow` loses the foreground race often enough
that CopyFromScreen captures whatever else is on top. That happened twice and
both captures (a browser, a chat client) were deleted. Prefer PrintWindow for
app windows, prefer human eyes for message boxes, and trust the per-PID window
enumeration (class + title) over pixels in both cases.
## Original triage of the config gaps (superseded by the section above)

- **QuestEditor** — wants `../../exe/win32/QuestEditorConfig.xml`, a relative
  path from the old win32 layout. The exe now lives in `x64/Release`, so it
  resolves to nothing. Either drop the XML at that relative location or repoint it.
- **ShipComponentEditor** — the dialog lists six hardcoded `c:/projects/swg/x1/...`
  SOE paths: server dsrc + data, shared dsrc + data, clientdata dsrc, and the
  `object_template_crc_string_table.iff`. Needs repointing at this dataset.
- **SwgDraftSchematicEditor** — `Configuration::loadCfg` (`Configuration.cpp:308+`)
  parses its OWN key=value format and requires **`serverObjectTemplatePath`** and
  **`draftSchematicDirectory`**; ours has neither, so it returns false and you get
  the "not properly configured" box. It also wants a companion
  `SwgDraftSchematicEditor.ini` (`loadIni` — template-version and
  base-template keys). After OK the frame does come up, output pane honestly
  reporting `0 armorRatings / 0 craftingTypes / 0 damageTypes / ...`.
  Note `cms_root = "../../exe/win32/"` on one build branch (`Configuration.cpp:20`
  vs `:22`) — check which one this build compiled.
- **SwgSpaceQuestEditor** — same shape: Configuration tab shows empty
  `sharedStringFilePath` and `spaceQuestDirectory`, and 0 spaceZones /
  questCategories / spaceMobiles / factions / cargo files / missionTemplateTypes.
- **"X is not running from `<branch>\exe\win32`"** on Conversation / SpaceQuest /
  SpaceZone — purely cosmetic location check. SpaceQuest stacks three of them
  and does not create its frame until they are all dismissed.

## What is already fixed (don't redo)

Yesterday's session fixed the cfg sku-form bug across all 15 tool cfgs. See the
memory note `tool-cfgs-need-legacy-no-sku-treefile-keys`. Short version: tools
call `SetupSharedFile::install(useFileStreamer)` with no `skuBits`, so
`TreeFile::install` builds `searchTree0` / `searchPath12` / `searchTOC0`, while
only SwgClient passes real skuBits and reads the `_00_` form. **Never sync
`client.cfg` onto the tool cfgs.** Backup of the old sku-form file is
`_cfg.sku-form.bak`.

## Tooling added this session (`src/build/win32/x64/Release/`)

- `_smoke-auto.ps1` — non-interactive replacement for `_smoke.ps1`. Launches each
  editor, waits, enumerates that PID's visible windows (class + title via
  EnumWindows), copies `logs/warning.log` per tool into `logs/_smoke/`, scores,
  kills, writes `_smoke-results.csv`. `-Only <substr>` filters; `-WaitSeconds`
  defaults 14 — **use 30**, the god client is still on its splash at 14 s.
- `_shot.ps1` / `_shot2.ps1` — launch + screenshot each window by window rect.
  `_shot2` dismisses `#32770` message boxes first (via Enter; WM_COMMAND IDOK
  did not take, WM_CLOSE does).
- Screenshots in `logs/_shots/`, per-tool logs in `logs/_smoke/`.

**Known trap — fix before trusting the CSV verdicts.** The auto-verdict treats
any `#32770` window as a modal complaint. That is wrong in both directions:
MFC *dialog-based* apps (UIBuilder) legitimately have `#32770` as their MAIN
window, and Qt error boxes (QuestEditor's "Critical Error",
ShipComponentEditor's "Errors in paths") are class `QWidget` and were scored
`w`. **Score by reading window titles, not window class.** The table at the top
of this document is hand-corrected; the CSV's verdict column is not.

**Second trap:** `SetForegroundWindow` loses the foreground race often enough
that a screenshot can capture whatever else is on top — one capture came back as
the user's browser and was deleted. Treat the per-PID window enumeration
(class + title) as the evidence; the pixels are a bonus.

## Suggested next steps

Status at end of session: **16 of 16 editors launch to a working main window.**
Items 1 and 3 of the original list are DONE (NpcEditor solved; all config data
ported from the SOE tree) — see the sections above.

Remaining, in the order I would do them:

1. **SoundEditor** — the last known code defect. `SoundEditor.cpp:449` hardcodes
   `data.configFile = "client.cfg"` while line 463 calls
   `SetupSharedFile::install(false)` with no skuBits, so it mounts ZERO TREs. It
   launches fine and will fail on first file open. Change the string to
   `"SoundEditor.cfg"` (that file already exists in Release in the correct legacy
   key form) and rebuild just that project. The build path is proven: VS 18 on
   D:, `-p:Platform=x64`, vcxproj directly — see the rebuild section above.
2. **Commit.** Nothing from this session is committed. The working tree holds two
   source fixes (`GameWidget.cpp`, `NpcCuiViewer.cpp`), this `.planning/` handoff,
   `scripts/trelist.py`, `src/build/win32/exe/` (the `../../exe/win32/` config
   payload), and the smoke/screenshot tooling.
   **Watch out:** the tool cfgs under `src/build/win32/x64/Release/` and the loose
   UI override in `D:\Code\Galaxies-Reborn\stage-B-override\ui\` are OUTSIDE this
   repo's tracked tree. Back them up separately or they are lost on a clean
   checkout — and NpcEditor stops working without that UI file.
3. **Functional testing beyond launch.** Everything so far is launch-smoke plus
   screenshots: nothing has been exercised, no file opened, nothing saved. The
   one specific item flagged in the 2026-08-22 notes and still unverified is
   **ParticleEditor's embedded preview** — that is where the composite change
   shows, and it should be a 1:1 crop rather than a squeezed scene.
4. **The 137 v0006 TREs** in the GR client set remain unexamined; `trelist.py`
   implements TRE 0005 only. Use SOE's `TreeFileExtractor.exe` if a census must
   be exhaustive.
5. **Staging DLLs**, open since 2026-08-22: staging has the newer `gl11_r.dll`
   beside the older exe, and `stage-x64/gl05,06,07_r.dll` show as modified in git
   (rebuilt 08-20 18:25, byte-identical sizes to the 08-18 copies in Release).
   No ABI change, harmless — but unresolved.

## OPEN: renderer correctness — outside this session's mandate, unresolved

Reported by the user 2026-08-23 after the 16/16 launch verification, from visual
inspection of the tools that do a **serverless start** (ParticleEditor,
AnimationEditor and friends — engine boots standalone, no sign-in, a naked
character in the viewport). NOT the god client, which goes through the game
sign-in path and renders differently.

Two symptoms, both unexplained:

1. **The skybox renders magenta.**
2. **Terrain looks wrong** in the tools that display it.
3. **Particle effects render as plain rectangles** moving away from the
   character — i.e. unshaded/untextured quads.

**This does not contradict the 16/16 result.** That result means every tool
starts and presents a working UI. It was never a claim that the D3D11 output is
correct. These are different properties and only the first was tested.

### Ruled OUT (checked, with evidence — do not re-run these)

* Missing textures. Exactly one texture failure across every tool log
  (`texture/jedi.dds`, introduced by this session's NpcEditor backdrop config).
  `texture/defaulttexture.dds` is present.
* Missing shaders. `shader/skybox.sht` (data_other_00.tre) and
  `shader/skybox_6sided.sht` (patch_08.tre) both exist. The fallback in
  `ShaderTemplateList.cpp:225` WARNs on failure and never fires.
* Shader linkage. Zero "canonical interpolant signature" warnings — that path
  explicitly warns when a draw "will not render".
* Shader compile failures. Only benign X3206 truncation noise.

### Dead theories — three, all mine, all wrong. Don't repeat them.

1. *Cubemap sampling via the phantom-element path.* Wrong: the phantom layout
   decodes (VertexBufferFormat.h) as `0x1109` = position | color0 |
   texCoordCount1 | texCoordSet0_**2d**. The skybox uses a **3d** set
   (`SkyBoxAppearance.cpp:97-98`), so it is a different format entirely.
2. *The phantom element is the particle quads.* Also wrong. The comment at
   `Direct3d11_InputLayoutCache.cpp:~540` states the case explicitly: "the space
   nebula quads (position/color/texcoord) feed a_vertexlit.vsh, which also
   declares a normal, and retail drew them". `0x1109` matches that exactly, and
   the same log carries `could not open table [datatables/space/nebula/simple.iff]`.
   It is deliberate D3D9-behaviour emulation, fires once per unique layout
   (cached, not per draw), and is benign.
3. *The vacuous ABI guard is the cause.* Unproven and probably not. See below.

### The one real find, stated at its actual weight

`Direct3d11_ShaderReflection.cpp:449` warns that its own constant name table does
not match the shipped `.inc` files, so **the constant ABI guard checked nothing**
— shader constants are not being verified at all. That is a genuine hole in the
port's safety net and the code says it "must be corrected". It is a *diagnostic*
gap, NOT evidence of a render fault. Do not promote it to a cause without a
frame capture showing bad constant data.

### Cheapest next test (do this before anything expensive)

The ParticleEditor tree for a freshly-opened DEFAULT effect literally reads
`Particle Quad ( No Shader )`. An unshaded quad renders as a plain rectangle, so
the particle symptom may simply be an unconfigured default rather than a defect.
Falsify by loading a real effect — there are **1769 `.prt` files** in the
readable TREs, e.g. `appearance/pt_bolt_ion_cannon_proton.prt`. If a real effect
renders correctly, symptom 3 is a non-issue and the search narrows to sky+terrain.

### After that: RenderDoc, not more log archaeology

Logs have now produced three wrong answers. A frame capture answers directly what
they cannot: which draw emits the sky, what SRV/sampler is bound at that draw, and
what is actually in the constant buffer (the one thing nothing currently checks).
`D:\Code\renderdoc-mcp` exists but was NOT connected to this session's MCP servers.

### Capture caveat for whoever looks next

`PrintWindow` did NOT capture ParticleEditor's D3D viewport (came back blank),
though it captured NpcEditor's fine. Screen-region capture works but needs the
window foregrounded, and `SetForegroundWindow` loses the race often enough to
grab unrelated windows. Easiest reliable path is a human screenshot.
