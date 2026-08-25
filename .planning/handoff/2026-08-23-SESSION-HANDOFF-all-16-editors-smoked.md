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

### RESOLVED 2026-08-23: symptom 3 (particles as rectangles) was NOT a defect

Loading a real effect settles it. `pt_campfire_s01.prt` (from the SOE tree's
2101 loose .prt files at
`data/sku.0/sys.client/compiled/game/appearance/`) loads and renders correctly —
the character stands in a working campfire, confirmed visually by the user.

The tree for the loaded effect names a shader per emitter:

```
Particle Effect ( Default ) 86
  Emitter -> Particle Quad ( fire )
  Emitter -> Particle Quad ( trailing embers )
  Emitter -> Particle Quad ( trailing smoke )
```

versus the freshly-opened DEFAULT effect, which reads `Particle Quad ( No Shader )`.
An unshaded quad draws as a plain rectangle, so "a bunch of rectangles moving away
from the character" was the unconfigured default behaving correctly.

**The particle rendering path in the DX11 port is working.** Remaining unexplained:
the magenta skybox and the terrain appearance, symptoms 1 and 2 only.

How it was driven, for repeatability: ParticleEditor ignores argv
(`main.cpp` passes it to QApplication only) and opens via `QFileDialog`
(`MainWindow.cpp:292`), so it needs a loose file and UI automation —
foreground the main window, `Ctrl+O` (accel from `ui/BaseMainWindow.ui`
`fileOpenAction`), type the full path, Enter. Guard the keystrokes on
`GetForegroundWindow()` actually being the target, or they land in whatever else
has focus.

Two false signals to ignore when checking whether a load worked: a SUCCESSFUL
load writes nothing to `warning.log` (an unchanged log is not failure), and the
window caption updates late — it read `ParticleEditor_r` for a while after the
load before becoming `... : pt_campfire_s01.prt`.

## RenderDoc setup + capture attempts (2026-08-23, written just before a restart)

Goal: find why the skybox renders magenta and the terrain looks wrong in the
serverless-start tools. Symptom 3 (particles) is already resolved above and was
not a defect.

### MCP is registered — it should be LIVE after the restart

```
claude mcp add --scope user renderdoc \
  "D:\Code\renderdoc-mcp\v0.3.0\renderdoc-mcp-windows-x64-v0.3.0\bin\renderdoc-mcp.exe"
```

Written to `C:\Users\kenne\.claude.json`; `claude mcp list` reports it Connected.
It exposes **52 tools** (open_capture, list_draws, get_pipeline_state,
get_bindings, get_shader, list_resources, export textures, pixel debug, frame
diff...). MCP servers bind at session start, which is why the restart was needed.
**Check the tool list first thing** — if the renderdoc tools are present, prefer
them; they likely include attach-to-a-running-process, which solves the capture
problem described below.

### The CLI works without any MCP and is often faster

`D:\Code\renderdoc-mcp\v0.3.0\renderdoc-mcp-windows-x64-v0.3.0\bin\renderdoc-cli.exe`

```
renderdoc-cli <capture.rdc> <command> [options]
  info | events | draws [--filter T] | pipeline [-e EID] | shader STAGE [-e EID]
  resources [--type T] | export-rt IDX -o DIR [-e EID] | pixel X Y [-e EID]
  pick-pixel X Y | debug pixel X Y -e EID [--trace] | debug vertex VTX -e EID
  mesh EID [--stage vs-out] | snapshot EID -o DIR | usage RES_ID | tex-stats
  assert-pixel EID X Y --expect R G B A | assert-state | assert-image
  capture EXE [-w DIR] [-a ARGS] [-d N] [-o PATH]
```

### THE CAPTURE PROBLEM — read this before retrying `capture`

`capture -d N` waits N **presented frames** then grabs one. There is no
`--timeout` flag and the internal timeout is fixed. Results measured:

| -d N  | outcome |
|-------|---------|
| 120   | SUCCEEDED, but the frame is mid-load: **2 events, 1 draw** — useless |
| 400   | timed out |
| 700   | timed out (even with the window force-foregrounded) |
| 1500  | timed out |
| 2500  | timed out |

Not linear, and the log says why:
`Direct3d11: no compiled shader manifest at 'compiled_shader/manifest.txt';
every program will be compiled at first use.` The first frames that actually
draw the scene each stall compiling shaders, so the frame rate collapses exactly
when the scene appears. "Late enough to show the sky" and "fast enough to beat
the timeout" do not overlap.

Ideas for next time, roughly in order:
1. Use the MCP's own capture tool — it may attach to an already-running process,
   which removes the timing race completely.
2. **Build the shader cache first** so frames stop stalling. The engine wants
   `compiled_shader/manifest.txt`; `ShaderBuilder.exe` exists in the SOE tree at
   `D:\SWG All Tools Working\swg\current\exe\win32\`. With a warm cache a much
   larger `-d` should complete.
3. Manual capture: `qrenderdoc.exe` IS installed at `C:\Program Files\RenderDoc`.
   Launch ParticleEditor from its Launch Application tab, wait until the magenta
   sky is visible, press **F12**. Then analyse the .rdc with the CLI — the
   analysis is the valuable part and needs no automation.

Also note: the app must be **foregrounded** to accumulate frames at a useful rate
(a background window presents very slowly). Force it with ShowWindow/
BringWindowToTop/SetForegroundWindow in a loop while the capture waits.

### CLI quirk that will waste your time

`-o foo.rdc` actually writes **`foo_frame0.rdc`**, then the CLI looks for
`foo.rdc` and reports `error: Capture completed but file not found on disk`.
The capture succeeded — look for the `_frame0` suffix. Confirm against the
RenderDoc log (`%TEMP%\RenderDoc\RenderDoc_<date>.log`), which prints
`Written to disk: <path>`.

### The one capture taken is NOT worth keeping

`pe_skybox_frame0.rdc` (3.6 MB, D3D11, 2 events / 1 draw) is a mid-load frame
with no scene. It lives in this session's scratchpad
(`%TEMP%\claude\D--Code-swg-qt-tools-worktree\<session-id>\scratchpad\`) which is
session-scoped and will be orphaned by the restart. No loss — recapture.

### The analysis plan, once a capture WITH the scene exists

1. `info` — sanity check event/draw counts (a real scene frame will have many).
2. `draws` — find the sky draw. It should be near the start of the frame,
   full-screen, likely depth-write off.
3. `pipeline -e EID` — what is bound: render targets, depth state, and crucially
   the SRVs.
4. `get_bindings` / `resources --type Texture` — is a cubemap actually bound at
   that draw, or the default texture? `SkyBoxAppearance.cpp:97-98` sets
   `setNumberOfTextureCoordinateSets(1)` +
   `setTextureCoordinateSetDimension(0, 3)`, i.e. a 3-component (cube) lookup;
   `SkyBox6SidedAppearance.cpp:271` instead builds six `texture/%s_%s.dds` names.
5. `shader ps -e EID` — the pixel shader for that draw.
6. `pixel X Y -e EID` — get the colour NUMERICALLY. This finally answers "is it
   literally magenta" instead of judging a screenshot.
7. `debug pixel X Y -e EID --trace` — trace the invocation that produced it.
8. If it turns out to be a constant-binding problem, remember the port already
   reports its **constant ABI guard is vacuous**
   (`Direct3d11_ShaderReflection.cpp:449`) — constants are not verified by
   anything, so a frame capture is the only source of truth for what is bound.
9. When the cause is found, `assert-pixel` / `assert-state` turn it into a
   regression check rather than something a human has to eyeball.

### Machine state at restart

No editor processes left running (all ParticleEditor instances stopped). The
working tree is clean and everything is committed. `warning.log` in Release holds
the last injected run; per-tool logs are in `logs/_smoke/`, screenshots in
`logs/_shots/`.

## SOLVED 2026-08-24: the magenta sky is a CLEAR COLOUR, not a failed skybox

Capture `D:\Code\Galaxies-Reborn\stage-B-x64\Capture250.rdc` (D3D11, 26 events,
21 draws, 1280x720) — a real scene frame, taken manually with F12 after the
automated `capture -d N` route proved unworkable (see the capture-problem section
above; the MCP's `capture_frame` is launch-and-delay only, it cannot attach, so
that escape does not exist).

### What the capture proves

`pixel_history` at (640,60) — sky region — on the scene target `ResourceId::214`
returns **exactly one modification for the whole frame**: event 10, a **Clear**,
postMod `(1.0, 0.0, 1.0, 1.0)`.

**There is no skybox draw at all.** Not a mis-sampled cubemap, not a broken
shader, not a bad SRV binding — nothing is drawn there, and what shows through is
the clear colour. Every theory in the "Dead theories" section above was aimed at
the wrong thing, and so was the analysis plan's step 4 (look for a cubemap at the
sky draw): that draw does not exist.

Also established from the same capture, so nobody re-checks:

* The frame is structurally sane: clear (10, 11), scene draws (48..340), one
  3-index fullscreen composite (354) into backbuffer `ResourceId::840`, Present.
* **The composite is innocent.** It faithfully copies 214 -> 840. Its shader
  output at the sky pixel is already magenta because its input is.
* **Non-sky pixels are fine.** (640,400) = `0.63, 0.53, 0.47` (tan),
  (200,650) = `0.43, 0.47, 0.28` (green). Geometry and texturing work.
* Draw bindings are ordinary mesh shaders (`samplerDiffuse0` / `samplerNormal0`);
  the 6-index draws at 271..340 are the particle quads (single sampler `s0`).

### Where the magenta comes from — exact source

`GroundScene.cpp:2344,2371` clears to `terrainObject->getClearColor()`, which is
`ClientProceduralTerrainAppearance::getClearColor` ->
`GroundEnvironment::getClearColor` -> `m_clearColor`, interpolated
(`GroundEnvironment.cpp:1096`) from the environment block's clear-colour ramp.

`EnvironmentBlock::loadColorRamps` (`EnvironmentBlock.cpp:715`) begins

```cpp
Image* const image = fileName ? ImageFormatList::loadImage (fileName) : 0;
if (image && isValid (image))
    ... load the real ramps ...
else
    ... default ramps, including:
    m_clearColorRamp [i] = PackedRgb (255, 0, 255);   // line 811 — MAGENTA
```

`PackedRgb(255,0,255)` is `(1.0, 0.0, 1.0)` — **bit-for-bit the captured pixel**.
It is a deliberate "these ramps did not load" sentinel.

### Why nothing was ever logged — and why the log hunt kept failing

The only diagnostic on that path is

```cpp
DEBUG_WARNING (fileName && *fileName, ("...not in the appropriate format (256w x 8h x 32b tga)"));
```

Two independent reasons it is silent here:

1. `DEBUG_WARNING` **compiles out in release** (`Fatal.h:50` — the same trap the
   NpcEditor `strlen(NULL)` fix hit; use `WARNING` for anything release-visible).
2. The `fileName && *fileName` guard suppresses it **entirely** when the name is
   empty — which is exactly the default-environment-block case.

So the engine takes a documented failure path and says nothing at all. That is
why sessions of log archaeology produced wrong answers: the evidence was never in
the log to begin with.

### Ruled OUT this session, with evidence — do not re-run these

* **TGA loader not installed.** It is. `GameWidget.cpp:271-273` calls
  `SetupSharedImage::setupDefaultData` then `install`, and
  `setupDefaultData` sets `m_supportTarga = true` (`SetupSharedImage.cpp:67`),
  which registers `TargaFormat`. Same in NpcEditor / TerrainEditor /
  ShipComponentEditor.
* **Ramp files missing from the mounted TREs.** They are present:
  `trelist.py "D:/Code/SWGSource Client v3.0" colorramp` finds them in
  `data_other_00.tre`, `patch_00.tre`, `patch_01.tre`, `patch_06.tre`
  (`terrain/colorramp/*.tga` plus loose `terrain/*_colorramp_*.tga`).
* **Ramp files invalid.** Extracted `terrain/colorramp/tatooine_global0.tga`
  and parsed its header: **256w x 8h x 32bpp, uncompressed (imgtype 2)** — 8236
  bytes = 18 header + 8192 pixels + 26 TGA-2.0 footer. That satisfies
  `isValid()` (`EnvironmentBlock.cpp:61`: width 256, height 8 or 10, 32-bit
  format). The data is good.

### What is NOT yet established

Which of the **two** entry conditions into that else-branch actually fires:

* **(a) `fileName` is null/empty** — the block is
  `EnvironmentBlockManager::m_defaultEnvironmentBlock`, which has no
  `colorRampFileName`. This is the likelier one: it also explains the silence
  (the guard suppresses the warning) and it means the serverless tools simply
  never resolve a real environment family, because no planet/terrain environment
  is loaded. Under this reading it is a **scene-setup/content gap in the tools,
  not a DX11 port defect**, and the DX11 renderer is exonerated.
* **(b) the image genuinely fails to load** at runtime despite being valid on
  disk — a TreeFile/streaming problem specific to the tools' mount.

These are cheap to separate and the next session should do exactly that before
anything else.

### Next step — one decisive test, ~15 minutes

Temporarily promote the diagnostic and rebuild ParticleEditor:

```cpp
// EnvironmentBlock.cpp, else-branch of loadColorRamps
WARNING (true, ("EnvironmentBlock::loadColorRamps FALLBACK - fileName=[%s] image=%p",
                fileName ? fileName : "(null)", image));
```

`WARNING` (not `DEBUG_WARNING`) survives release. One run then says which case it
is, and names the file if there is one. Branch (a) -> fix where the tools pick
their environment family; branch (b) -> chase the TreeFile mount.

Worth confirming in the same run: **does the god client show a correct sky?** If
yes, the D3D11 sky path is provably fine and the fault is entirely in how the
serverless tools set up their scene. That single observation splits the problem
in half and costs one launch.

### This also likely explains symptom 2 (terrain looks wrong) — same root cause

The magenta clear is only the most visible of the defaults. The **same**
else-branch replaces every lighting ramp at once
(`EnvironmentBlock.cpp:798-818`): ambient -> `solidGray`, main diffuse and
specular -> `solidWhite` at scale 1, fill and bounce -> `solidBlack`, fog ->
`solidGray`, shadow -> `solidGray`, back/tangent -> `solidBlack`.

That is precisely a flat, wrongly-lit, unfogged world. **Symptoms 1 and 2 are one
bug, not two** — which fits the observation that both appear together in exactly
the serverless-start tools. Fixing the ramp load should be expected to fix both;
if it fixes the sky but not the terrain, then and only then treat terrain as
separate.

### Regression check once fixed

The capture makes this mechanical — `assert-pixel` on the sky region:
`renderdoc-cli <capture.rdc> assert-pixel 354 640 60 --expect <r> <g> <b> 1`.
Any future regression to `1 0 1 1` is caught without a human eyeballing a
screenshot.

## 2026-08-24 (later): the WARNING test ran — answer is EMPTY_NAME, and the cause is `terrain/simple.trn`

The test proposed above was carried out. Result: **case (a)**, and it leads to a
one-line config cause with no DX11 involvement at all.

### What was done

`EnvironmentBlock.cpp`, else-branch of `loadColorRamps`, gained a release-visible
`WARNING` splitting the three possible entry conditions (EMPTY_NAME /
LOAD_FAILED / BAD_FORMAT). Rebuilt `clientTerrain.vcxproj` then
`ParticleEditor.vcxproj`:

```
MSBuild: D:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe
  -p:Configuration=Release -p:Platform=x64 -m
```

Both succeeded (clientTerrain 92 warnings / 0 errors, ParticleEditor 3 / 0).

### Result — unambiguous

```
WARNING b160e5df: EnvironmentBlock::loadColorRamps FALLBACK case=EMPTY_NAME
(default environment block; no colour ramp configured) -> magenta clear
```

Counts over the whole run: **EMPTY_NAME 1, LOAD_FAILED 0, BAD_FORMAT 0.**

So the ramp files were never even *asked* for. Nothing failed to load and
nothing was malformed — which retroactively confirms the three "ruled out"
findings above were correct, and rules the TreeFile/streaming branch (b) out
entirely.

The same log shows terrain shaders compiling and running normally
(`dot3_terrain_imp1`, `terrain_dot3_vs20_blend0`), so terrain *geometry* was
always fine; only its lighting environment was defaulted.

### The actual cause — ParticleEditor loads a bare test terrain

`Game.cpp:859-874`: for `A_particleEditor` / `A_animationEditor`, the scene comes
from `ConfigClientGame::getParticleEditorGroundScene()`, and
`ConfigClientGame.cpp:1088` defines it as

```cpp
ms_particleEditorGroundScene = ConfigFile::getKeyString("ParticleEditor", "groundScene", "terrain/simple.trn");
```

Our `ParticleEditor.cfg` has **no `[ParticleEditor]` section at all**, so the
compiled default applies: **`terrain/simple.trn`** — a bare test terrain with no
environment family. No environment family means no environment block, so
`EnvironmentBlockManager` hands back `m_defaultEnvironmentBlock`, which has an
empty `colorRampFileName`, which takes the fallback, which paints the sky
magenta. Every step is now accounted for.

`terrain/simple.trn` is real and present (`data_other_00.tre`); so is
`terrain/tatooine.trn` (`patch_01.tre`).

### Confirming test — pointing it at a real planet

Appended to `src\build\win32\x64\Release\ParticleEditor.cfg` (backup:
`_ParticleEditor.cfg.pre-groundscene.bak`):

```
[ParticleEditor]
	groundScene=terrain/tatooine.trn
```

The run changes character completely: **912 -> 1991 log lines**, and it now
compiles **`vertex_program/stars.vsh` + `pixel_program/stars.psh`** and the
`water_pass2_ps20` pair — celestial and water programs that the `simple.trn` run
never touched. The sky/celestial machinery only engages when a terrain actually
carries an environment.

**Do not misread the remaining EMPTY_NAME line in that run.** It still appears,
and that is correct and harmless: `EnvironmentBlockManager` *always* constructs a
default block with no ramp, independently of how many real blocks load. The
warning proves the default block exists; it does not prove real ones are absent.
The `stars.*` compiles are the evidence that real ones loaded.

### THIS IS PROBABLY NOT A PORT DEFECT — and probably not a defect at all

SOE's own `ParticleEditor.cfg` (reference tree, `exe/win32/`) sets no
`groundScene` either — it is 25 lines and only sets `windowed`, `uiRootName`,
`loadHud=false`, `preloadWorldSnapshot=0`, and includes `tools.cfg` (which also
sets none). So **SOE's ParticleEditor also ran on `terrain/simple.trn` and would
also have shown the magenta clear.** The magenta sky looks like original
behaviour for a bare tool scene, not a DX11 regression.

Which means the DX11 port is **exonerated for symptom 1**, and "fixing" it is a
matter of choosing a nicer default scene for the tools rather than repairing any
rendering code.

### VISUALLY CONFIRMED by the user, 2026-08-24

> "The magenta is gone and I see the tatooine surface, sky box looks a bit
> oversaturated, but could be lighting issue."

That closes the investigation:

* **Symptom 1 (magenta skybox) — RESOLVED.** Cause was the default
  `groundScene=terrain/simple.trn`, a bare test terrain with no environment
  family. Not a DX11 port defect; almost certainly original SOE behaviour.
* **Symptom 2 (terrain looks wrong) — RESOLVED, same root cause**, as predicted.
  The tatooine surface renders. Both symptoms were one bug, and that bug was a
  missing config key rather than any rendering code.

The DX11 port is **exonerated for both**. Every renderer theory from the earlier
sessions (cubemap sampling, the phantom vertex element, the vacuous constant ABI
guard) was chasing a fault that did not exist.

### NEW, SMALL open item: the sky looks oversaturated

Reported in the same observation. This is a **fresh, much narrower** item — the
sky now draws, it just may not be graded right. Candidate causes, none yet
tested: gamma / sRGB handling in the DX11 port, time-of-day position in the
colour ramp (`GroundEnvironment.cpp:1096` interpolates by `m_currentColorIndex`),
or simply how tatooine's ramp looks at that hour.

**Do not theorise this one from source** — that is the mistake that cost this
investigation several sessions. Two cheap measurements first:

1. Re-capture with F12 and read the sky pixel numerically (`pick_pixel`), then
   compare against the corresponding entry in tatooine's colour-ramp TGA. If the
   drawn value matches the ramp, the renderer is right and it is content/hour.
2. Compare against the god client on the same planet — it goes through the same
   `GroundEnvironment` path but a different startup. A matching look there means
   nothing is tool-specific.

Note the tools still run without a compiled shader manifest, so this is also the
moment to check whether anything colour-related differs with a warm cache.

### Code left in place (deliberate, not scaffolding)

The diagnostic was **kept**, but reshaped so it does not become log noise:

* `LOAD_FAILED` and `BAD_FORMAT` -> release-visible `WARNING`. These indicate a
  genuine fault and were previously silent in release; that silence is exactly
  what cost this investigation several sessions.
* `EMPTY_NAME` -> `DEBUG_WARNING` only, because it fires once in every run of
  every tool and the client by design. Keeping it at `WARNING` would have made
  every release log dirty.

`clientTerrain` was rebuilt after the reshape. **ParticleEditor still needs a
relink** to pick it up — it was left running for a visual check and the exe was
locked.

### Loose ends for the next session

1. Relink ParticleEditor (`ParticleEditor.vcxproj`, Release|x64).
2. Decide whether `groundScene=terrain/tatooine.trn` should be kept. It is
   currently in the Release cfg as a TEST with a backup beside it. Note that cfg
   lives OUTSIDE the tracked tree — see the warning about that in the commit
   section above.
3. Confirm visually whether terrain now looks right; that closes or re-opens
   symptom 2.
4. `AnimationEditor` shares the identical code path (`A_particleEditor ||
   A_animationEditor`) and so has the same default and the same magenta sky. Any
   decision here should be applied to both.

## 2026-08-24 (final): groundScene made permanent for BOTH editors, both relinked

### The section name is a trap — it is `[ParticleEditor]` for both tools

There is **no** AnimationEditor equivalent of this key. `Game.cpp:859` handles
`A_particleEditor || A_animationEditor` in one branch and calls
`ConfigClientGame::getParticleEditorGroundScene()` for both, and
`ConfigClientGame.cpp:1088` reads it from section **`"ParticleEditor"`**.

So `AnimationEditor.cfg` needs a `[ParticleEditor]` section. An
`[AnimationEditor]` section would be **silently ignored** — no error, no warning,
the compiled default just keeps applying and the sky stays magenta. The same
holds for `avatarSelection` (`ConfigClientGame.cpp:1089`).

### What was written

The identical block was put in **both** `ParticleEditor.cfg` and
`AnimationEditor.cfg` in `src\build\win32\x64\Release\`, replacing the
provisional TEST block in the former:

```
[ParticleEditor]
	groundScene=terrain/tatooine.trn
```

(with a comment block recording why, the shared-section trap, and the
consequence of omitting it). Backup of the pre-change ParticleEditor cfg:
`_ParticleEditor.cfg.pre-groundscene.bak`.

Both files verified afterwards: sections intact
(`SharedFile / Station / ClientGraphics / SharedFoundation / SharedLog /
ParticleEditor`), and `diff` against the backup shows the appended block as the
only change.

### Rebuilt and verified

`clientTerrain.vcxproj` (for the reshaped diagnostic), then
`ParticleEditor.vcxproj` and `AnimationEditor.vcxproj` — Release|x64, 0 errors
each. Exes relinked 14:54:58 and 14:55:10.

Smoke run of both, 55 s each:

| | ParticleEditor | AnimationEditor |
|---|---|---|
| starts and stays up | yes | yes |
| log lines | 1990 | 2019 |
| `loadColorRamps` warnings | **0** | **0** |
| `stars.vsh` / `stars.psh` compiles | **3** | **3** |

Zero `loadColorRamps` warnings is the expected result, and it means two things
at once: no real ramp fault occurs (`LOAD_FAILED` / `BAD_FORMAT` would now be
release-visible), and the benign `EMPTY_NAME` case is correctly debug-only so it
no longer dirties the release log.

The `stars.*` compiles in **both** confirm the sky/celestial path now engages in
AnimationEditor too — i.e. the shared-section behaviour works as read.

### STILL OUTSTANDING: these cfgs are NOT in the repo

`.gitignore:118` ignores `**/build/win32/x64/`, so every tool cfg under
`src\build\win32\x64\Release\` is untracked — including both files just edited.
**A clean checkout loses all of them**, and with them this fix, the NpcEditor
config work, and the six ported SOE tool sections.

`git ls-files src/build/win32/` returns only 15 entries: the vcxproj/sln files
and the tracked `exe/win32/` payload. Nothing from `x64/Release/`.

This was flagged in the earlier commit section and is still open. It is a
decision, not an oversight — the options are to un-ignore the cfgs, to keep a
tracked snapshot directory, or to accept the loss and rely on this handoff. The
key itself is reproduced verbatim above so it is at least reconstructible.

## 2026-08-24 (final+1): the tool cfgs are now TRACKED — the clean-checkout hole is closed

The open item recorded above ("these cfgs are NOT in the repo") is resolved.
**21 config files** — 16 `.cfg` and 5 `.ini` under
`src\build\win32\x64\Release\` — are now tracked.

### Why the .gitignore change looks the way it does

Three rules covered that path and **all three excluded directories**:

```
src/build/win32/x64/
**/build/win32/x64/
**/x64/Release/
```

Git **never descends into an excluded directory**, so no `!` negation inside
them can ever work. Simply appending `!.../Release/*.cfg` would have done
nothing at all — silently. The rules therefore had to be rewritten to exclude
directory *contents* instead:

```
src/build/win32/x64/**
**/build/win32/x64/**
**/x64/Release/**

!src/build/win32/x64/
!src/build/win32/x64/Release/
!src/build/win32/x64/Release/*.cfg
!src/build/win32/x64/Release/*.ini
```

The two intermediate-directory negations are load-bearing: without them git
stops at `x64/` and never sees the files, no matter what the file patterns say.

**If anyone "tidies" these back into bare directory rules, the tool configs
silently stop being tracked again.** A comment in `.gitignore` says so.

### Verified, not assumed

* `git status --untracked-files=all` on that directory: **exactly 21 files**,
  all `.cfg` / `.ini`.
* Spot-checked `git check-ignore`: `ParticleEditor.cfg`, `AnimationEditor.cfg`,
  `SwgSpaceQuestEditor.ini` -> tracked; `ParticleEditor_r.exe`,
  `_cfg.sku-form.bak` -> still ignored.
* Build output still ignored: `.pdb`, `logs/warning.log`, `_smoke-results.csv`.
* Repo-wide untracked count afterwards: 22 = the 21 configs + `.gitignore`
  itself. Nothing else was loosened.

### What this preserves

Everything a clean checkout used to destroy: the legacy no-sku TreeFile keys
across all tool cfgs, the six ported SOE tool sections, the NpcEditor config
work, and the `[ParticleEditor] groundScene` key that keeps the sky from
rendering magenta.

### CAVEAT — these files contain machine-specific absolute paths

This is the real cost of tracking them, and it should be understood rather than
discovered later. The cfgs point at **this machine's** layout:

* `D:/Code/SWGSource Client v3.0/...` (the TRE set)
* `D:/SWG All Tools Working/swg/current/...` (the SOE reference tree)
* `D:/Code/Galaxies-Reborn/stage-B-override/ui/` (the NpcEditor `/AvView` page)

On any other machine these resolve to nothing, and the failure mode is mostly
**silent** — zero TREs mounted, or counts quietly reading 0 — not an error
dialog. So treat the tracked copies as *this machine's working set plus a
recoverable record of the structure*, not as portable configuration.

If these ever need to work on a second machine, the fix is to parameterise the
roots rather than to hand-edit 21 files; note also the quoting trap recorded
earlier in this document (`ConfigFile` splits unquoted values on whitespace, and
`D:\SWG All Tools Working` contains spaces).

`client.cfg` is among the tracked files. It is the CLIENT config and carries the
`_00_` sku key form. **Never copy it onto a tool cfg** — see the memory note
`tool-cfgs-need-legacy-no-sku-treefile-keys` and the landmine section above.

## 2026-08-24: the oversaturated sky is BLOOM, and the pink is a clamped overflow

Capture `D:\Code\Galaxies-Reborn\stage-B-x64\Capture251.rdc` (D3D11, 351 events,
**346 draws**, 1280x720) — the tatooine scene, taken with F12. Compare with
Capture250's 21 draws: the environment really is loading now.

### The frame's actual structure

| stage | events | target |
|---|---|---|
| scene | 10 (clear) .. ~4030 | `212` (B8G8R8A8_UNORM) |
| bloom downsample | 4033 | `223` (320x180) |
| bloom blur H/V | 4050, 4055 -> `228`; 4065, 4070 -> `230` | 320x180 |
| **bloom composite** | **4088** (reads `212` + `230`) | `214` |
| fog/copy | 4099 (reads `214`) | `48` |
| present blit | 4113 (reads `48`) | `960` swapchain |

### It is NOT a gamma/sRGB problem — that theory is dead

I floated `B8G8R8A8_UNORM` vs `_SRGB` last session as something the capture could
test. **It tested false.** The pale pink is not a colour-space conversion, it is
an arithmetic **overflow that clamps**:

At the sky pixel (640,50), the bloom composite at 4088 outputs
`(1.3608, 0.8078, 0.9353, alpha 2.0)` and the UNORM target clamps it to
`(1.0, 0.808, 0.933)`. Red went past 1.0 and got cut. A clamped red channel with
green and blue left high is exactly "washed-out pink". Nothing is being
mis-converted; something is being added twice.

### The mechanism, read off the shaders

The scene's **alpha channel is the bloom mask**. Two shader disassemblies prove
it:

*Downsample (4033):* accumulates `rgb += sample.rgb * sample.a` over 16 taps and
divides by 16. RGB is weighted **by alpha**, so a pixel only contributes bloom in
proportion to its alpha.

*Composite (4088):* `result.rgba = bloom.rgba * bloom.a + scene.rgba`.

Measured values confirm the mask is working as designed:

| pixel | bloom texture `230` | composite out at 4088 |
|---|---|---|
| sky (160,12) | `1.0, 0.612, 0.710, a=1.0` | `1.361, 0.808, 0.935, a=2.0` -> clamped |
| ground (160,150) | `0.0, 0.0, 0.0, a=1.0` | `0.396, 0.239, 0.224, a=1.0` unchanged |

Bloom **alpha** is 1.0 everywhere; it is bloom **RGB** that is zero over the
ground and bright over the sky. That is the downsample's alpha weighting doing
its job: ground pixels carry alpha ~0, sky pixels carry alpha 1.0.

So the sky's bloom is a blurred copy of a uniformly-alpha-1.0 sky, added back at
full weight — roughly **doubling** it. Hence the overflow. The ground is
untouched, which matches the report exactly: sky oversaturated, terrain fine.

### Sanity check against the authored data

`terrain/colorramp/tatooine_global0.tga` row 5 (the clear-colour ramp) holds
muted values — brightest `145,190,213`, most around `74,74,93`. The captured
scene target agrees: clear `74,55,70`, after the sky dome `111,60,71`. **Dark and
muted all the way through the scene pass.** Everything vivid appears after 4088.
The authored content is not the problem.

### A REAL port deviation found on the way — clear alpha is hardcoded

`Direct3d11_SwapChain::clearViewport` builds its clear colour as

```cpp
float const color[4] = {
    ((colorValue >> 16) & 0xff) / 255.0f,   // R
    ((colorValue >>  8) & 0xff) / 255.0f,   // G
    ((colorValue      ) & 0xff) / 255.0f,   // B
    1.0f };                                 // A  <-- hardcoded
```

The alpha byte `(colorValue >> 24) & 0xff` is **discarded**. Direct3d9 passed
`colorValue` straight to `ms_device->Clear(...)` as a D3DCOLOR, alpha byte
included — and `GroundScene.cpp:2371` clears with `backgroundColor.asUint32()`
where `PackedRgb::asUint32()` (PackedRgb.h:80) returns

```cpp
(r << 16) | (g << 8) | b        // alpha byte is ZERO
```

So retail cleared the scene alpha to **0** ("do not bloom"), and the port clears
it to **1.0** ("bloom at full strength"). Every pixel not overdrawn by geometry
is therefore marked for maximum bloom. This is a genuine, independently
verifiable D3D9->D3D11 behavioural deviation and it is a one-line fix.

### HONEST STATUS — do not treat the clear as proven to be the cause

The clear is **a** cause of sky alpha being 1.0. It is not yet proven to be
**the** cause at this pixel, because the sky dome draw itself (EID 33, 336
indices, single texture, into `212`) also outputs **alpha 1.0** — its
`shaderOut` is `(0.435, 0.234, 0.280, 1.0)`. It draws over the whole dome after
the clear, so at these pixels the clear's alpha may be irrelevant unless D3D9
masked alpha writes for that material.

Two possibilities remain and they need separating before anything is called
fixed:

* **(i)** the clear alpha alone, in which case the one-line fix is sufficient;
* **(ii)** the sky material's alpha write / colour-write mask is not being
  honoured in the port, in which case the clear fix changes nothing at the dome.

### Next step — cheap and decisive, in this order

1. Fix `clearViewport` to decode alpha from `colorValue` rather than forcing
   1.0. It is correct regardless of (i) vs (ii) — the port currently cannot
   express "clear alpha to 0" at all.
2. Rebuild, re-capture, and read the scene target's alpha at a sky pixel.
   Alpha 0 -> case (i), done. Alpha still 1.0 -> case (ii), and the search moves
   to the sky shader's alpha output and the D3D11 blend/write-mask handling.
3. Regression check once fixed: `assert-pixel` at 4088 asserting the composite
   output no longer exceeds 1.0 at a sky pixel.

Do not skip step 2. The measured fact that the dome writes alpha 1.0 itself is
exactly the sort of detail that made the earlier magenta theories wrong.

## 2026-08-24: clear-alpha FIXED and verified — case (i), the clear was the sole cause

### The fix

`Direct3d11_SwapChain::clearViewport` now decodes the alpha byte instead of
forcing 1.0:

```cpp
static_cast<float>((colorValue >> 24) & 0xff) / 255.0f   // was: 1.0f
```

Checked first that this was the only such site in the D3D11 port — the other
`{1.0f, 1.0f, 1.0f, 1.0f}` arrays are blend factors
(`Direct3d11_ShaderImplementationData.cpp:32`, `Direct3d11_StateCache.cpp:243`)
and an input-layout default, not clears.

Rebuilt `Direct3d11.vcxproj` (Release|x64, 0 errors) -> `gl11_r.dll` in the
Release dir. ParticleEditor smoke run afterwards: clean, 1990 log lines, no
FATAL/ERROR lines.

### It was case (i) — and the write-mask question is now ANSWERED

The previous entry left open whether the sky dome's own alpha-1.0 output would
defeat the clear fix. **It does not.** From the new capture at the sky pixel:

| event | before fix | after fix |
|---|---|---|
| 10 (clear) postMod alpha | 1.0 | **0.0** |
| 33 (sky dome) shaderOut alpha | 1.0 | 1.0 (unchanged) |
| 33 (sky dome) **postMod** alpha | 1.0 | **0.0** |

The dome still *outputs* alpha 1.0, but the render target's alpha is **not**
updated — so the colour-write mask **is** correctly masking alpha for that
material in the port. Nothing is wrong with the sky shader or the write-mask
handling. The clear was poisoning the bloom mask, and only the clear.

That is a satisfying result because it also retires the suspicion, raised
earlier, that the port might be mishandling write masks.

### End-to-end verification, all from the capture

| measurement | before | after |
|---|---|---|
| scene alpha at sky | 1.0 | **0.0** |
| bloom texture at sky | `1.0, 0.612, 0.710, a=1.0` | **`0, 0, 0, a=0`** |
| bloom composite shaderOut | `1.361, 0.808, 0.935, a=2.0` (**overflow, clamped**) | **`0.361, 0.196, 0.227, a=0.0`** (no overflow) |
| final backbuffer at sky | `1.0, 0.808, 0.937` = `255,206,239` pink | **`0.369, 0.196, 0.231` = `94,50,59`** muted |

The composite output now equals the scene value passed through untouched, which
is exactly right for a pixel that should not bloom.

### CAVEAT on the before/after colour numbers

The two captures are ~14 minutes apart and ParticleEditor runs a **time-of-day
cycle** (`ParticleEditorIoWin.cpp:365` only pauses it when `timeOfDayCycle` is
off), so the environment has moved between them. **Do not read the absolute RGB
before/after as a like-for-like comparison** — the ground pixel also changed
(`102,61,58` -> `64,46,43`) purely from the time shift.

What *is* time-independent, and is the actual proof: alpha 0 instead of 1.0,
bloom contribution 0 instead of full, and no overflow at the composite. Those
hold regardless of the hour.

### AUTOMATED CAPTURE NOW WORKS — the handoff's idea #2 is confirmed

This capture was taken by the **MCP's `capture_frame`, not by hand**, after
copying the baked shader cache into the Release directory:

```
D:\Code\Galaxies-Reborn\stage-B-x64\compiled_shader\  ->  ...\x64\Release\compiled_shader\
```

206 files / 231-line manifest. With the cache warm, `delayFrames: 900`
**completed** and produced a full 54.5 MB, 346-draw scene frame — where before
the fix every attempt above 120 frames timed out and 120 caught a useless
2-event mid-load frame.

So the capture problem that blocked several sessions is solved: **warm the
shader cache first, then `capture_frame` with a large `delayFrames`.** No human
F12 needed. The cache is safe to leave in place — blobs are keyed by a hash of
the exact compile input, so any mismatch simply recompiles rather than loading a
stale shader.

The `-o foo.rdc` -> `foo_frame<N>.rdc` quirk still applies and still reports a
false "file not found on disk". Here it wrote `alphafix_frame336.rdc`.

### Suggested regression check

`assert-pixel` at the bloom composite asserting the sky pixel's output stays
below 1.0. That turns "the sky is blown out" into a numeric check that does not
need a human to eyeball a screenshot, and it is immune to the time-of-day drift
that makes absolute colours unreliable.

### Remaining

Visual confirmation by eye is still worth doing — the numbers are conclusive
about the overflow, but only a human can say whether the sky now *looks* right.

## 2026-08-24: trelist.py now reads v0006 — and the 137-archive open item is CLOSED

### Do not reverse-engineer this format — there is a reference implementation

The user pointed at **SWG-Toolkit**, which has a full TRE reader set. The
authoritative sources on this machine:

* `D:/Code/swg-client-v2/tools/tre-lint/src/format.ts` — zero-dependency TS,
  every layout verified against real bytes and engine source.
* `D:/Code/SWG-Toolkit/.planning/handoff/2026-08-17-PROVIDER-NOTE-tre-lint-seed-and-v0006-verdict.md`

Check these FIRST for anything TRE-format-shaped. The v0006 record layout in
particular was *arbitrated on real bytes*, not guessed.

### The v0006 layout, and the model that was falsified

Header is the same 36 bytes as v0005 (`token@0 version@4 numberOfFiles@8
tocOffset@12 tocCompressor@16 sizeOfTOC@20 blockCompressor@24 sizeOfNameBlock@28
uncompSizeOfNameBlock@32`, all LE). Only the TOC record differs:

```
v0005 -- 24 B: crc@0 length@4 offset@8 compressor@12 compressedLength@16 fileNameOffset@20
v0006 -- 32 B: crc@0 length@4 offset@8 zero@12 zero@16 fileNameOffset@20 compressor@24 compressedLength@28
```

The v0006 form is the **REORDER** model. A competing **pad** model (compressor /
compressedLength at 12/16, padding at 24..31) is **FALSIFIED** — it scored 0.0%
on every populated archive while reorder won unanimously across all 46 populated
Restoration archives. Do not swap them back. Note the GR engine's own
`TreeVersion.h`/`TreBuilder` carry the pad assumption and are therefore wrong.

Compressor codes: `0` stored, `1` zlib (Restoration dialect), `2` zlib (stock).
Both 1 and 2 are plain zlib.

Tags are big-endian-composed uint32s written little-endian, so a hex dump shows
them mirrored: `TREE`->`EERT`, `0005`->`5000`, `0006`->`6000`. That mirroring is
NOT corruption and not a byte-order bug.

### The 137 v0006 archives contain NOTHING — item closed

```
scanned 209 tre(s) [v0005: 72, v0006: 137], 217533 entries, 137 empty, 0 unreadable
```

Zero unreadable, and the entry total is **unchanged** from the v0005-only count.
Audited every v0006 header directly: **all 137 have an entirely zero header body**
— `numberOfFiles`, `tocOffset`, every field. They declare nothing. This is not a
parse failure and not a limitation of the reader.

So the standing worry that "~two thirds of the corpus was never examined" is
**resolved: there was nothing in it to examine.** Any earlier census that covered
only v0005 archives was, for this corpus, already complete. (The populated v0006
archives the provider note describes belong to the *Restoration* corpus, which is
not installed here.)

Some v0006 files do carry payload — `hotfix_59_client_00.tre` is 1.38 MB of
concatenated zlib streams holding `PEFT` particle IFFs — but with no TOC
indexing them they are containers, not mountable archives, exactly as the
provider note's "container/Restoration; not searchTree-mountable stock" says.

### colorRampFileName, chased for kicks — and a loose end worth recording

`datatables/environment/tatooine.iff` is a `DTII` DataTable, 25 columns, 85 rows.
Column 9 is `Lighting Color Ramps (256x8 tga)` = `CD_colorRampFileName`. Parsed
it fully (row walk consumed exactly 34272 of 34272 bytes, which validates the
layout: `s` = inline NUL-terminated, `I`/`f` = 4 bytes LE).

Tatooine uses exactly **two** ramps across all 17 families:
`tatooine_global0.tga` (weather 0-1) and `tatooine_global1.tga` (weather 2-4).

**The four copies of `tatooine_global0.tga` all DIFFER** (data_other_00,
patch_00, patch_01, patch_06 — four distinct md5s). Any analysis must use the
highest-priority copy; here that is **patch_06**.

The TGA is bottom-origin, and the loader flips it, so image row `r` = file row
`7-r`. That mapping is self-consistent against the known ramp semantics: bounce
= `0,0,0`, fill = `2,1,1`, mainDiffuse = `164,88,47` (a warm orange — exactly
right for twin suns).

**The loose end:** the frame's measured clear colour was `74,55,70`. Under that
mapping the *clear* ramp at the matching time index is `96,88,106` (muted
blue-grey), while `74,55,70` matches the **fog** ramp at the same index to within
one bit of blue (`74,55,69`). Searching every row and all 256 time indices, the
fog row is the only near-exact match; the clear row never takes that value.

That is an observation, **not a diagnosis** — the obvious mechanism is absent:
`GroundScene.cpp:2371` clears to `terrainObject->getClearColor()`,
`ClientProceduralTerrainAppearance::getClearColor` returns
`GroundEnvironment::m_clearColor`, and `GroundEnvironment.cpp:1096` builds that
purely from the clear ramps with nothing modifying it afterwards. So there is no
code path visible that would substitute fog for clear.

Possible and untested: the flip assumption is wrong in some way the semantic
cross-check does not catch; or the environment block in play is not one of these
two files; or something reorders the ramp rows on load. **Cosmetically this does
not matter** — the user judged the result "close enough" and the sky renders
correctly — so this is logged as a curiosity for anyone who wants it, not as an
open defect.

### trelist.py

Rewritten. Reads v0005 and v0006, treats compressor 1 and 2 both as zlib,
reports empty-but-valid archives as empty rather than as errors, prints a
per-version census, and exposes `find()` / `extract()` for import. Usage
unchanged: `python scripts/trelist.py <dir> [substring] [--dump [outdir]]`.

# ===== SESSION END 2026-08-24 — POWER-LOSS SHUTDOWN — START HERE =====

Written under an imminent power cut. **Everything below the line is committed and
safe.** Working tree was clean at the last commit.

## What this session did (7 commits, all pushed to the branch)

| commit | what |
|---|---|
| `d80c55ae9` | magenta sky root-caused via RenderDoc — it is the CLEAR COLOUR; no skybox draw exists |
| `03c006ad0` | `EnvironmentBlock`: warn in release when a colour ramp fails to load |
| `2a368d829` | `groundScene` made permanent for ParticleEditor **and** AnimationEditor |
| `9d2a534ba` | tool cfgs TRACKED — 21 config files no longer lost on a clean checkout |
| `18d589c75` | oversaturated sky root-caused: bloom overflow, not gamma |
| `de6bf4161` | **Direct3d11: decode clear alpha instead of forcing it to 1.0** (the real port fix) |
| `81dc6b286` | `trelist.py` reads TRE v0006; the 137-archive question CLOSED |

**Both original renderer symptoms are closed.** Magenta sky + wrong terrain were
one bug — a missing `groundScene` config key, not a port defect. The
oversaturated sky WAS a genuine D3D11 port defect and is fixed and verified.

## IN FLIGHT AND LOST — re-run this first

A full 16-tool smoke run (`_smoke-auto.ps1 -WaitSeconds 30`) was running in the
background when the shutdown came. **Its results are lost — re-run it.**

It matters more than usual this time: today's `gl11_r.dll` (clear-alpha fix) and
`clientTerrain` (ramp diagnostic) rebuilds sit underneath **every** editor with a
3D viewport, and nothing has confirmed the other 15 tools still launch cleanly
since. Treat the 16/16 result as UNVERIFIED against today's binaries.

```
cd D:\Code\swg-qt-tools-worktree\src\build\win32\x64\Release
.\_smoke-auto.ps1 -WaitSeconds 30
```

Score by reading window TITLES, not window class — the CSV verdict column is
still wrong in both directions (see the trap noted earlier in this document).

## NEXT STEPS, in the order I would do them

### 1. Track `ui_root_npceditor.ui` — the last clean-checkout hole (2 minutes)

`NpcEditor.cfg` is now tracked and points at
`searchPath12="D:/Code/Galaxies-Reborn/stage-B-override"`, but the 11 KB file it
needs — `stage-B-override/ui/ui_root_npceditor.ui` — is **NOT tracked and lives
outside the repo**. A clean checkout therefore produces a tracked cfg pointing at
a missing file, and NpcEditor FATALs again on the absent `/AvView` page.

Precedent for tracking it already exists: `NpcEditor.tab` and
`QuestEditorConfig.xml` under `src/build/win32/exe/win32/` are tracked SOE
originals. Do the same for this file. Verified present this session:
11,349 bytes, dated 2026-08-23 19:37.

### 2. SoundEditor — the shipped fix is HALF-TESTED

`SoundEditor.cpp:449` was changed from `client.cfg` to `SoundEditor.cfg`, rebuilt
and committed (`d17043a48`) — but **the entire point of that fix was first-file
open**, and no sound file has ever been opened. Its known defect ("launches fine,
mounts zero TREs, fails the moment you open a sound file") is invisible to a
launch smoke by construction.

7,677 `.snd` files are available, e.g. `sound/music_combat_loop.snd`
(`ILM_music.tre`). Open one. This is the highest-value untested thing in the
tree.

### 3. AnimationEditor — newly changed, never exercised

It received the `[ParticleEditor] groundScene` key this session (it reads that
section, not an `[AnimationEditor]` one — see the trap recorded above) and shares
ParticleEditor's serverless path. Smoke-passed, but nothing loaded. 7,765 `.ans`
files available. Drive it the same way ParticleEditor was driven (Ctrl+O, type
the path, Enter, guarded on `GetForegroundWindow()`).

### 4. Functional testing generally — the big untested surface

**Only ParticleEditor has ever been driven past launch** (one `.prt` load).
Nothing else has opened or saved a file. Several tools display data parsed at
boot (SwgDraftSchematicEditor's 749 resourceTypes, SpaceQuest's 20 categories) —
that is startup parsing, not editing.

### 5. Do NOT "fix" these

* `SwgConversationEditor` / `SwgSpaceZoneEditor` scoring `c`: the
  `<branch>\exe\win32` check is structural and can never match unless run from
  `.../swg/<branch>/exe/win32`. Cosmetic, one click.
* QuestEditor's blank component icons: `data/internal/.../questeditor/image/`
  simply does not exist here. Documented, not fatal.

### 6. Still open from earlier sessions

Staging DLLs (08-22): staging has the newer `gl11_r.dll` beside the older exe and
`stage-x64/gl05,06,07_r.dll` show as modified. No ABI change, harmless,
unresolved. **Note today's rebuild produced a new `gl11_r.dll` in Release**, so
re-check this with fresh eyes rather than against the old notes.

## Two things that changed how to work here — do not miss these

**Automated frame capture now WORKS.** Copy the baked shader cache into the tool's
directory first:

```
D:\Code\Galaxies-Reborn\stage-B-x64\compiled_shader\  ->  <tool dir>\compiled_shader\
```

(already done for `x64\Release`). Then the RenderDoc MCP's `capture_frame` with
`delayFrames: 900` completes and yields a full scene frame. No human F12 needed.
`pixel_history` is the single highest-value call — it answers "what wrote this
pixel" directly and settled both bugs this session.

**SWG-Toolkit is the reference for anything TRE-format-shaped.** Do not
reverse-engineer:
* `D:/Code/swg-client-v2/tools/tre-lint/src/format.ts`
* `D:/Code/SWG-Toolkit/.planning/handoff/2026-08-17-PROVIDER-NOTE-tre-lint-seed-and-v0006-verdict.md`

## Machine state at shutdown

ParticleEditor may still be running (PID 3024) plus whatever the interrupted
smoke run left; kill any `*Editor_r.exe` / `SwgGodClient_r.exe` before trusting a
fresh run. `warning.log` in Release holds the last smoke tool's output, not
ParticleEditor's. Per-run logs saved this session:
`logs/warning.pre-envtest.log`, `logs/warning.envtest-simple.log`,
`logs/warning.envtest-tatooine.log`.

Captures worth keeping: `D:\Code\Galaxies-Reborn\stage-B-x64\Capture250.rdc`
(magenta, pre-fix) and `Capture251.rdc` (oversaturated, pre-alpha-fix). The
post-fix capture is in this session's scratchpad and will be orphaned — recapture
is now cheap, so no loss.

## LATE ADDENDUM — the interrupted smoke got through 9/16 before it was killed

Partial output survived. Amends the "UNVERIFIED" warning above: the first **9 of
16 tools are confirmed alive against TODAY'S binaries** (the clear-alpha
`gl11_r.dll` and the rebuilt `clientTerrain`).

| # | tool | verdict |
|---|------|---------|
| 1 | SwgGodClient_r | w |
| 2 | TerrainEditor_r | w |
| 3 | UIBuilder | `?` — known false negative, its `#32770` IS the main window |
| 4 | ParticleEditor_r | w |
| 5 | AnimationEditor_r | w |
| 6 | LightningEditor_r | w |
| 7 | SwooshEditor_r | w |
| 8 | NpcEditor_r | w — full title `Npc Editor (Aug 20 2026 - 18:51:54) : Default` |
| 9 | SoundEditor_r | w |

That covers the three that mattered most for regression risk: **both editors
changed this session** (ParticleEditor, AnimationEditor) and **NpcEditor**, the
most fragile tool in the set — all still reach a real main window. No regression
from the D3D11 clear-alpha fix is visible in the first 9.

**Still to confirm: tools 10-16** — ClientEffectEditor, QuestEditor,
ShipComponentEditor, SwgConversationEditor, SwgDraftSchematicEditor,
SwgSpaceQuestEditor, SwgSpaceZoneEditor. Re-run the smoke to close those out; it
is the first task next session either way.


# ===== SESSION 2026-08-24 (resume after power loss) =====

## TerrainEditor's "Tip of the Day" — the tips file was NEVER SHIPPED. Closed.

Asked whether the tips exist in the official editor config area. They do not,
anywhere. This is not a port defect and there is nothing to port.

The dialog wants a file called `terrainEditor.tip`, opened with a plain `fopen`
relative to the CWD (`TipDialog.cpp:39`) — **not** through TreeFile, so no
searchPath, TRE or cfg key can ever supply it. It has to sit literally beside
the exe.

Searched, all negative:

| Location | `*.tip` |
|---|---|
| `D:\SWG All Tools Working` — the whole SOE reference tree | none |
| `D:\SWG All Tools Working\swg\current\exe\win32` | ships only `TerrainEditor.exe` + `TerrainEditor.ini` |
| this repo + `D:\Code\Galaxies-Reborn` stage dirs | none |

So SOE's own build showed the same dialog we do. What is displayed is the
failure branch: string resource `CG_IDS_FILE_ABSENT` = *"Tips file does not
exist in the prescribed directory"* (`TerrainEditor.rc:2520`).

Two traps worth remembering:

* **`TerrainEditor.ini` is NOT the MFC profile.** It is the shader/family list
  data (`SF/dirt/dirt_bigcracks=...`, ~229 KB). Do not go looking for a `[Tip]`
  section in it.
* The real profile is the **registry**:
  `HKCU\Software\Sony Online Entertainment\TerrainEditor`, section `[Tip]`,
  keys `StartUp` / `FilePos` / `TimeStamp` (`TerrainEditor.cpp:331` sets the
  registry key). Checked this session: the `Tip` subkey **does not exist**, so
  `GetProfileInt(...,"StartUp",0)` returns 0, `m_bStartup` stays TRUE, and the
  dialog fires on every launch. Ticking off "Show tips on startup" once writes
  `StartUp=1` and it never comes back. That is the entire fix if it is wanted —
  no code change, no file to author.

Also note `ShowTipAtStartup()` only runs when `cmdInfo.m_bShowSplash` is set,
i.e. when the editor is launched with no file argument — which is exactly what
the smoke does.

## `ui_root_npceditor.ui` is now TRACKED — clean-checkout hole closed

Handoff next-step #1, done. Two changes:

1. The file is copied (byte-identical, 11,349 bytes) to
   `src/build/win32/exe/win32/ui/ui_root_npceditor.ui`, beside the other
   preserved SOE originals (`NpcEditor.tab`, `QuestEditorConfig.xml`). No
   `.gitignore` change was needed — verified with `git check-ignore`, that
   directory is not excluded.
2. `NpcEditor.cfg` gained `searchPath11="../../exe/win32"`, **relative**, which
   resolves from the Release dir back to that preservation store.

Why relative works, and it is worth knowing generally:
`TreeFile::SearchPath`'s constructor runs `Os::getAbsolutePath()` on whatever
the cfg gives it (`TreeFile_SearchNode.cpp:114`), resolving against the CWD —
the exe's own directory. So relative searchPaths are legal and are the way to
avoid the machine-specific-absolute-path problem flagged earlier in this doc.

**VERIFIED, not assumed** — by negative control, which is the only way this is
worth anything:

| run | out-of-repo copy | result |
|---|---|---|
| control | present | ALIVE at 45s, no FATAL |
| test, first attempt (`searchPath13`) | hidden | **EXITED 0x80000003**, `FATAL ExceptionHandler invoked` |
| test, after fix (`searchPath11`) | hidden | ALIVE at 45s, no FATAL |

**THREE searchPath traps, and the middle one cost real time:**

* A searchPath that does **not exist** is a hard `FATAL`, not a warning —
  `FATAL(!result, ("Could not convert to absolute path. Does it exist? %s"))`
  at `TreeFile_SearchNode.cpp:117`. That is why the missing-directory case bites
  so hard on a clean checkout.
* **`maxSearchPriority` CAPS WHICH KEYS ARE EVEN READ, and this cfg sets it to
  12** (`NpcEditor.cfg:5`). The registration loop is
  `for (priority = 0; priority <= maxPriority; ++priority)` (`TreeFile.cpp:133`),
  so `searchPath13` was **silently never read** — no warning, no error, the key
  simply does nothing. The library default is 20 (`TreeFile.cpp:117`), which is
  what misled me; the cfg overrides it. **If a searchPath appears to do nothing,
  check `maxSearchPriority` FIRST.** Priorities in use here: TOC 0-3, Tree
  0,2-8, Path 12. Free below the cap: 1, 9, 10, 11.
* Higher number = **higher** precedence: nodes are sorted with
  `a->getPriority() > b->getPriority()` (`TreeFile.cpp:336`), descending. So 11
  deliberately sits *below* stage-B-override's 12 — the override corpus keeps
  precedence and the in-repo copy is purely the safety net.

`stage-B-override` is deliberately LEFT IN PLACE at priority 12 — it is the
asm2hlsl corpus and carries `appearance/`, `datatables/`, `pixel_program/`,
`snapshot/`, `texture/`, `vertex_program/` besides this one `ui/` file.

## The 16-tool smoke is RE-RUN and GREEN against today's binaries

The run lost to the power cut is redone in full. `_smoke-auto.ps1 -WaitSeconds 30`,
results in `src/build/win32/x64/Release/_smoke-results.csv`.

**16/16 alive. No FATAL in any per-tool log, no crash artifacts.** This closes the
"UNVERIFIED against today's binaries" warning: the clear-alpha `gl11_r.dll` and the
rebuilt `clientTerrain` sit under every 3D-viewport editor and nothing regressed.

Tools 10-16, the ones the interrupted run never reached, all came up:

| # | tool | verdict | window title |
|---|------|---------|--------------|
| 10 | ClientEffectEditor_r | w | `ClientEffectEditor_r` |
| 11 | QuestEditor_r | w | `QuestEditor Version 2.11 (Built Aug 20 2026 - 18:17:05)` |
| 12 | ShipComponentEditor_r | w | `ShipComponentEditor_r` |
| 13 | SwgConversationEditor_r | w | `SwgConversationEditor` (MFC frame) |
| 14 | SwgDraftSchematicEditor | w | `SwgDraftSchematicEditor` (MFC frame) |
| 15 | SwgSpaceQuestEditor_r | w | `SwgSpaceQuestEditor - [naboo_imperial_4.tab]` |
| 16 | SwgSpaceZoneEditor_r | w | `SwgSpaceZoneEditor` (MFC frame) |

### Two deltas from the documented baseline — neither is a regression

* **SwgConversationEditor and SwgSpaceZoneEditor now score `w`, not `c`.** The
  `<branch>\exe\win32` complaint dialog did not appear in the 30s window this
  run. **Cause NOT established** — do not record this as fixed. Note those two
  (and QuestEditor, SwgDraftSchematicEditor, SwgSpaceQuestEditor, UIBuilder)
  wrote **no `warning.log` at all** this run, so there is no log evidence either
  way; only 11 of 16 tools produced one. Worth one look if it matters.
* **UIBuilder still scores `?`** — unchanged known false negative. Its `#32770`
  IS its main window (MFC dialog-based app). Ignore.

Per-tool logs kept in `src/build/win32/x64/Release/logs/_smoke/`. Note
`UIBuilder.warning.log` there is STALE (16:39, an earlier run) — the script only
copies a log if the tool wrote one, so a stale file is not evidence.

### CRLF fidelity checked too — no `.gitattributes` needed

The repo has no `.gitattributes` and `core.autocrlf=true`, so a clean checkout
writes this file with **CRLF**, not the LF bytes that were tested. Rather than
assume, the CRLF form was generated (11,349 -> 11,380 bytes) and run with the
out-of-repo copy hidden: **ALIVE at 45s**. The UI parser does not care, so the
file is left as an ordinary tracked text file, consistent with the other
preserved SOE originals. Both copies were restored and verified byte-identical.

## SoundEditor first-file open — TESTED AT LAST, and the fix HOLDS

Handoff next-step #2, the "highest-value untested thing in the tree". The
`SoundEditor.cpp:449` cfg fix (`d17043a48`) had never been exercised, because its
whole point is first-file open and no sound file had ever been opened.

**It works. The tool opens a sound template and resolves every sample out of the
TREs.**

### How it was driven

`fileOpen()` uses `QFileDialog::getOpenFileNames` (`SoundEditor.cpp:648`), so it
opens a **loose .snd from the filesystem** — not from a TRE. There are no loose
`.snd` files in this repo or the stage dirs, but the SOE tree has **7,220** of
them under `swg/current/data/sku.0/sys.client/compiled/game/{sound,player_music/sound,voice/sound}`.

Menu accel is **Ctrl+O** — `BaseSoundEditor.ui` stores it as the integer
`4194383`, which is `0x400000 | 0x4F` = CTRL + 'O'. The file dialog is a native
`#32770` "Open Sound", not a Qt widget, so it takes a typed absolute path fine.
Driver kept at `scratchpad/_drive-soundeditor.ps1`.

### The prediction was made BEFORE the run, which is what makes the result mean anything

Target: `sound/music_combat_loop.snd` — an `SD2D` (Sound2dTemplate) referencing
**28** `music/mus_*.mp3` samples. All 28 were confirmed present in TREs that
`SoundEditor.cfg` actually mounts (checked by intersecting the cfg's 70
`searchTree0` entries against `scripts/trelist.py`; e.g.
`music/mus_combat_f_lp.mp3` -> `patch_15_00.tre`). Note **ILM_music.tre is NOT
mounted** by SoundEditor.cfg, so the base patch archives are doing the work.

* PASS looks like: 28 samples listed with real sizes.
* FAIL looks like: 28 x `WARNING: Unable to find the sample in the tree file
  path and it will not be loaded` (`SoundTemplateWidget.cpp`, the `else` of
  `TreeFile::exists` at line 441).

### Result — PASS, on two independent readings

1. Title `Sound Template - D:/SWG All Tools Working/.../music_combat_loop.snd`,
   and **"Sample List (28 samples @ 27174 KB)"** — 28/28, with per-sample sizes
   (680 KB, 1062 KB, 901 KB, ...). Sizes come from `Audio::getSampleSize()`,
   which reads through TreeFile, so non-zero sizes already prove the mounts.
2. Better: the **Audio Debug** panel reports `Cache Miss Count 28` /
   `Cached Sample Count 28`. The audio subsystem actually pulled all 28 mp3
   payloads out of the archives. That is impossible with zero mounted TREs.

Template fields also populated correctly (Play Order "Random No Repeats",
Priority 2, Category "Background Music", play count 99/99).

`warning.log` stayed at its usual 6 lines of JUCE/WASAPI init, no FATAL. **Do not
read that as evidence either way** — `SoundEditorUtility::report()` writes ONLY to
the GUI output window (`SoundEditorUtility.cpp:110`), never to a file. Any future
SoundEditor check has to be visual.

Screenshots: `logs/_shots/SoundEditor_{a-before,b-dialog,c-after,e-output}.png`.

### Playback CONFIRMED BY THE USER, 2026-08-24

`Play` was pressed by Kenny on the loaded `music_combat_loop.snd`: **"It plays."**

So the whole chain is now proven end to end — cfg read -> 70 TREs mounted ->
template parsed -> 28 samples resolved through TreeFile -> mp3 payloads decoded
and cached -> audio out through the JUCE 8.0.14 / WASAPI backend. Nothing about
SoundEditor's first-file-open path is outstanding.

This also makes SoundEditor the **second** tool ever driven past launch, after
ParticleEditor.

## AnimationEditor driven past launch — works, and `groundScene` is confirmed visually

Handoff next-step #3. It is up and rendering. PID left running for the user.

### The handoff's own lead was WRONG — correct it

This doc said "7,765 `.ans` files available. Drive it the same way ParticleEditor
was driven." **AnimationEditor does not open `.ans` files.** Its document types
are `.ash` (animation state hierarchy) and `.lat` (logical animation table) —
`MainWindow.cpp:351`, filter `Editor Files (*.ash *.lat *.xml)`. The `.ans` files
are the compiled animations a LAT *points at*, exactly as `.mp3` samples are what
a `.snd` points at. There are **203** loose `.ash`/`.lat` in the SOE tree under
`.../compiled/game/appearance/{ash,lat}/`.

### The file dialog is a dead end here — do not waste time on it

`openFile()` refuses anything not already under a TreeFile **search path**:

    if (!TreeFile::stripTreeFileSearchPathFromFile(...)) {
        WARNING(true, ("User: the specified file [%s] is not mappable to your
                       TreeFile path.  Fix path before opening."));
        return;   // MainWindow.cpp:363
    }

and `stripTreeFileSearchPathFromFile` only matches `SearchPath` nodes — it
`dynamic_cast`s to `SearchPath` and ignores every TRE (`TreeFile.cpp:1027`).
AnimationEditor.cfg has exactly **one** searchPath, `stage-B-override` at 12. So
pointing the dialog at the SOE tree is rejected by design; the file would first
have to be copied under stage-B-override. The `Open Shared Creature Template`
path (`MainWindow.cpp:108`) has the same constraint plus two more: the path must
contain the substring `shared` and must contain `.iff`.

### The route that needs none of that

`File -> Open Target ASH(s)` / `Open target LAT(s)` act on
`AnimationEditorGameWorld::getFocusObject()`, which falls back to
`networkScene->getPlayer()` — no dialog, no path mapping. The File menu has **no
accelerators at all**, so drive it by arrows: Alt+F then Down xN, where
1=Open Shared Creature Template, 2=New Ash, 3=New Lat, 4=Open..., 5=Open Target
ASH(s), 6=Open target LAT(s), 7=Close, 8=Save. Driver kept at
`scripts/_drive-animationeditor.ps1`.

### Result

**It loads the player's animation data BY ITSELF at boot** — no menu action
needed. At ~35s three tabs are already open:

    ASH - all_b.ash  |  LAT - all_m.lat  |  LAT - hum_m_face.lat

with the Logical Animation Mapping tree populated from
`Template Name: appearance/ash/all_b.ash` — add_face_blink, add_pistol_fire_1,
coup_de_grace, default, emt_accept_affection, emt_afk, emt_applause_excited,
emt_beckon, emt_belly_laugh, emt_bow2..5, emt_celebrate, ... real data out of the
TREs.

**Be accurate about the menu picks: they appear to have been no-ops.** The tab set
is byte-for-byte identical across the boot shot and both post-menu shots, which is
expected — the targets were already open. What changed between the boot shot and
the next one is the **3D viewport**: at 35s it was bare ground, and a few seconds
later it renders the **human male player character standing on dirt terrain**.
That is the skeletal mesh finishing its build, not the menu.

So `groundScene` for AnimationEditor (`2a368d829`, written into the
`[ParticleEditor]` section — see the section-name trap above) is now **visually
confirmed**, not just assumed from a launch smoke.

No FATAL, no `Failed to get the player or a target object from the NetworkScene`,
no `Could not get the NetworkScene`, no `not mappable` — checked in
`logs/warning.animationeditor-drive.log`.

Screenshots: `logs/_shots/AnimationEditor_{a-boot,b-lat,c-ash}.png`.

### ANIMATION PLAYBACK CONFIRMED (user, 2026-08-24) — and here is exactly how

**Double-click an Action in the `ASH - all_b.ash` tab, under the Actions
sub-folder.** Kenny did it and the character animated.

That is precisely the code path:

    void ActionListItem::doDoubleClick()   // ashFormat/ActionListItem.cpp:122
    {
        // GOAL: tell the focus Object in the game to do the current action.
        ...
        appearance->getAnimationResolver().playAction(m_action.getName(), ...);
    }

`MovementActionListItem::doDoubleClick()` (MovementActionListItem.cpp:123) is
identical. Both are **ashFormat** types.

**The LAT tabs cannot play anything — do not try.** `LogicalAnimationTableWidget`
forwards a double-click to `ListItem::doDoubleClick()`, whose base implementation
is literally `// Default: do nothing.` (`core/ListItem.cpp:213`). Nothing in
latFormat overrides it except `AnimationPriorityListItem::PathListItem`, which is
a path edit, not playback. So the `Logical Animation Mapping` tree — the one that
is open by default and looks like the obvious place to click — is a dead end for
playback.

Layout for whoever comes next: tab `ASH - all_b.ash` -> sub-tabs
`State Hierarchy` | `Action Groups`. The Action items carry a blue **A** icon
(dance_lyrical, dive, door_knock, duck, face_blow_kiss, ...).

**Honest note on the automated attempt:** a scripted double-click was fired at a
blue-A row in the `State Hierarchy` sub-tab and frame differencing over the
viewport could NOT distinguish it from idle (0.02-0.15% changed pixels vs a
0.06-0.07% idle baseline — the idle breathing animation sets that floor). Either
the click missed a real Action item or it landed in the wrong list. The scripted
route is unproven; the human route above is confirmed. If anyone wants this
automated, target the **Actions sub-folder** specifically and expect the idle
animation to make naive frame-diffing weak — compare limb silhouettes, or pick a
large-amplitude action, rather than counting changed pixels.

### Still not exercised

Nothing has been edited or saved in AnimationEditor.

## 2026-08-24: FULL PASS over every tool -> docs/TOOLS-GUIDE.md

Drove every editor that had never been opened past its launch screen, then wrote
`docs/TOOLS-GUIDE.md` (commit `537e5e4ce`): required setup, config keys, basic
instructions, in-app points of interest, and an honest assessment per tool.

**13 of 16 are now driven past launch, up from 3.** Read the guide rather than
duplicating it here; only the things that change how you work are repeated below.

### Config fixes made, each verified by a before/after run

| file | change | symptom it fixed |
|---|---|---|
| `LightningEditor.cfg` | `[ParticleEditor] groundScene` | magenta sky |
| `ClientEffectEditor.cfg` | `[ParticleEditor] groundScene` | magenta sky |
| `ClientEffectEditor.cfg` | `searchPath11` = SOE client root | **could not open any file at all** |
| `QuestEditor.cfg` | `searchPath10` = SOE **server** root, `11` = client root | **FATAL on every quest open** |
| `uibuilder_searchpaths.cfg` | created | UIBuilder had no search paths |

### The four findings worth carrying forward

1. **All five Qt GameWidget tools call `Game::install(Game::A_particleEditor)`**
   (`GameWidget.cpp:384`). ParticleEditor, AnimationEditor, SwooshEditor,
   LightningEditor and ClientEffectEditor are all "the particle editor", all take
   the `setScene()` branch, and all read `[ParticleEditor] groundScene` from
   whatever cfg they loaded. The magenta sky was never particle-editor-specific.
   SwooshEditor was accidentally fine only because it loads `AnimationEditor.cfg`.

2. **QuestEditor needs SERVER data.** It FATALed on
   `datatables/item/master_item/master_item.iff`, which is in **none** of the 209
   client TREs. A client-only mount list can never satisfy it. This is the
   clearest example of why the launch smoke was worth so little: it booted
   perfectly every single time and died on the first file opened.

3. **`stripTreeFileSearchPathFromFile` ignores TREs** (`TreeFile.cpp:1027`) — it
   only matches `searchPath` entries. So AnimationEditor and ClientEffectEditor
   refuse files that are mounted and perfectly readable, and the only trace is one
   line in `warning.log`. In the UI the dialog just closes and nothing happens.

4. **The SOE loose data roots are the master key.**
   `.../data/sku.0/sys.{client,server}/compiled/game` are valid TreeFile roots,
   named exactly as the TREs name their entries. Mounting one makes everything
   under it both loadable and mappable. **Cost: startup goes from ~20s to 60s+.**

### TerrainEditor: two things that will waste your time

* Its MFC profile is at the **doubled-Software** registry path —
  `HKCU\Software\Software\Sony Online Entertainment\TerrainEditor	errainEditor\`
  — because `SetRegistryKey` was passed a full path where MFC wants a company name.
* **Tip of the Day is modal and blocks all scripted input.** Set
  `...	errainEditor\Tip\StartUp` (DWORD) `= 1` to stop it. Verified. Also note
  `Ctrl+O` proved unreliable to script; the `Alt+F`, `O` menu route works, and
  there is **no command-line file open** (`ProcessShellCommand` is commented out).

### Still unproven, and stated plainly

* **Nothing has been saved or edited by any tool.** Every result is a read path.
* **SwgConversationEditor has no data here at all** — its document type is `.cnv`
  and there is not one `.cnv` file on this machine. Only the compiled output
  (1,363 `.java` conversation scripts) shipped. Not a port defect.
* **SwgSpaceQuestEditor and SwgSpaceZoneEditor were not driven** past their
  startup warnings.
* **SwgGodClient stops at the client LOG IN screen** — it is a full game client
  and needs a running server. Its UI and renderer are intact, which is real
  evidence the port is sound, but no editing feature can be assessed here.

### The two startup dialogs on the MFC Swg* tools are STRUCTURAL - do not "fix"

1. "not running from `<branch>\exe\win32`" — checks the CWD for the substring
   `exewin32` after stripping slashes. From `x64/Release` it can never match.
2. "running out of the 'qt-tools-worktree' branch which does not match ..." —
   compares the branch extracted from the CWD against the branch in the cfg paths
   (`current`). Cosmetic. `SwgSpaceZoneEditor.cpp:127,133,140`.

This also settles the earlier `c` -> `w` scoring question: the dialogs are still
there. The smoke scores on window TITLES and the dialog's title is just
`SwgConversationEditor`, which its error regex does not match. **Scoring
artifact, not a behaviour change.**

## 2026-08-24: the three "broken" MFC tools ALL WORK - I was wrong

Kenny pushed back ("I think they work more than you realize") and he was right.
None of the three was broken. Each just needed the correct way in, and I had
guessed wrong on all three. `docs/TOOLS-GUIDE.md` is corrected; **15 of 16 tools
are now driven past launch.**

### SwgSpaceZoneEditor - open the `.tab` SOURCE, not the compiled `.iff`

I had it backwards. `OnOpenDocument` calls `loadFromSpreadsheet()` on a `.tab`
and swaps the extension to `.iff` itself when it wants the compiled form
(`SwgSpaceZoneEditorDoc.cpp:392,859`). The cfg's `spaceZoneDataTablePath` points
at the compiled `.iff` directory, which is what misled me.

The sources are the **31** files under
`dsrc/sku.0/sys.server/compiled/game/datatables/space_zones/buildout/`.

`space_tatooine.tab` opens fine: tree of **Nav Points / Spawners /
Miscellaneous**, and the zone view plots real objects on a ruled XZ grid.

### SwgSpaceQuestEditor - it is a TREE BROWSER, not a File>Open tool

File>Open produces no dialog, which is why my driver failed. It does not need
one: the whole `spacequest` tree is **already loaded at boot** (20 categories)
with the Configuration tab parsed.

**Expand a category, double-click a `.tab` leaf.** Title becomes
`SwgSpaceQuestEditor - [<name>.tab]` and a full property editor appears -
`PT_navPointList`, `PT_spaceMobileList`, `PT_spawnerList`, a StringId/Text grid
and Quest Log Data. Verified on `_debug.tab`. (That file contains
`PT_notImplemented = ERROR(2): see asommers` - SOE test content, not a defect.)

### SwgConversationEditor - the TOOL works; only the documents are missing

`File > New` (`Ctrl+N`) creates `swgcon1` and gives the whole authoring
environment: Conversation Editor tree seeded `Root` -> `Npc Branch` with Add
Branch / Add Response / Test Branch, and a Script Editor with Conditions,
Actions, %TO/%DI/%DF Tokens, **Libraries** (ai_lib, chat, conversation, utils),
Labels, Triggers, Condition/Action Wizards, plus Spell Check, Scan and Compile
Debug/Release on the main toolbar.

The `.cnv` finding stands and is not a defect: `.cnv` is an IFF with FORM tag
`CNV` (versions 0000/0001/0002, `Conversation.cpp:1431`) and there is **not one
`.cnv` anywhere** - not under `D:\SWG All Tools Working`, not under `D:\Code`,
not in any of the 209 TREs. Only the compiled output shipped (1,363 `.java`
conversation scripts). So: working authoring tool, nothing authored to open.

**Precision note:** the four Libraries come from the new conversation's own
default `LibrarySet` (`ScriptTreeView.cpp:464`), not from a directory scan. Those
four names DO correspond to real files under `dsrc/.../script/library/`, so it is
*consistent with* a correct `scriptPath` - but it does not by itself prove
`scriptPath` resolved. Do not overstate it.

### The lesson worth keeping

Three tools, three different entry conventions, none of them File>Open with the
path the cfg advertises:

| tool | way in |
|---|---|
| SwgSpaceZoneEditor | File>Open a `.tab` under **dsrc**, not the cfg's `.iff` path |
| SwgSpaceQuestEditor | **double-click a leaf** in the tree it loads at boot |
| SwgConversationEditor | **File>New** - there is no data to open |

"Booted but not driven" was the right label at the time, but I let it read as
closer to broken than it was. When a tool looks inert, the entry convention is
the first thing to question, not the tool.

Driver added: `scripts/_drive-mfc.ps1` - launches, dismisses the modal startup
warnings (they swallow keys sent before dismissal), then File>Open with polling.
