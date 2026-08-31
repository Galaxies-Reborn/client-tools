# Installer design — single-root tools distribution (Kenny's direction, 2026-08-30)

Kenny: ship an installer that pulls from the git repos (dsrc, serverdata, a
NEW repo for the missing bits), lays everything out as one top-level
directory — apps + configs + data, loose or TRE per whatever makes sense —
and the applications work against that tree.

## Why the single root is stronger than it sounds: adopt the SOE SHAPE

Make the root SOE-shaped: `<root>\swg\current\{data,dsrc,exe}` with apps in
`exe\win32` (x64 binaries; the dir NAME is what matters). Three classes of
hardcoded behavior then work WITHOUT code changes:

1. The `../../exe/win32/` config reads (DraftSchematic, ConvEditor,
   QuestEditor xml, NpcEditor/Swoosh .tab — Configuration.cpp `cms_root`)
   resolve from `swg/current/exe/win32` back to... `swg/current/exe/win32`.
   Self-consistent by construction — that is exactly why SOE chose that
   relative path. The whole `src\build\win32\exe\win32\` mirror hack
   disappears.
2. `extractBranch` (SpaceQuest/SpaceZone branch-mismatch dialog, the one
   structural leftover) compares the segment after `swg/` in cwd vs cfg
   paths. One root + relative cfg paths under it = branches match = dialog
   gone legitimately.
3. Every cfg path becomes RELATIVE to the root (TreeFile searchPaths resolve
   against CWD; plain filesystem keys resolve against CWD too when the tools
   run from the exe dir). Result: **the final cfgs are machine-independent
   and can be TRACKED in git — the installer templates nothing.** The only
   configurable path left is the base-TRE directory, if it lives outside the
   root (see open questions).

## Layout sketch

```
<install>\swg\current\
  exe\win32\           all 16 editors + 10 CLI tools + cfgs/ini/xml/tab/dct/ps1
  data\sku.0\sys.client\compiled\game\   loose client data (serverdata clone + missing-bits repo)
  data\sku.0\sys.server\compiled\game\   REBUILT at install from dsrc
  data\sku.0\sys.shared\compiled\game\   REBUILT at install from dsrc
  dsrc\                SWG-Source/dsrc clone (editors read AND write here)
  tre\                 the 209 base TREs (or junction/config pointer to an existing client install)
  compiled_shader\     generated on first runs (or pre-warmed by installer)
```

stage-B-override and the stage-cee-loose junction DISAPPEAR — their contents
(ui_root_npceditor.ui, clienteffect browse dir) just live in data\. The
NpcEditor uiRootName override stays as a cfg key, now pointing into the tree.

## Loose vs TRE, per category (the shadow trap decides most of it)

Loose (must be):
- dsrc\ — read/write sources (git clone; git status doubles as write audit)
- Class B browse/write dirs: appearance/mesh (.lmg enumeration), quest/,
  string/en/, ui/, clienteffect/, server object/datatables/script/misc,
  shared object/datatables — see PAYLOAD-MANIFEST Class B table
- Rebuilt compiled output (it must be regenerable in place by the editors)

TRE (worth considering):
- The read-only payload that nothing browses via filesystem — chiefly the
  8,884 loose-only textures (1 GB) if we ship them: pack into e.g.
  `tools_texture.tre` with TreeFileBuilder (proven). One file, no shadow
  risk (contents curated), mounts via a searchTree key.
- Everything else is small enough that loose wins on simplicity.

Mount rule stands: loose searchPaths ONLY for scoped dirs; the broad
whole-root mounts (QuestEditor searchPath10/11) get scoped or replaced by
the packed TRE during cfg rework. searchPaths beat searchTrees at equal
priority (TreeFile.cpp:345) — order the keys deliberately.

## The new "missing bits" repo

Everything not obtainable from an existing repo or a rebuild:
- texture loose-only: 8,884 files / 1,042 MB (or the closure-trimmed subset;
  all small files, no LFS needed; ~1 GB repo is fine)
- small client leftovers (~26 MB): object/, ui/ (incl. ui_root_npceditor.ui),
  clientdata/, clienteffect/, sound/, interiorlayout/, abstract/, effect/,
  vertex_program/, templates/, input/, playback/, cockpit/
- media if wanted: sample/video/music/player_music/voice (651 MB) — decide
  drop vs include; SoundEditor browse dirs are the only likely consumer
- exe\win32 payload not in client-tools already: the two .dct dictionaries,
  godclient_favorites.xml (rest is tracked in client-tools exe/win32 store)
- the 41 size-mismatch files vs serverdata (triage: ours vs theirs)
- terrain: 1 not-covered file + planet .trn set is in TREs already

Generator: filter data-manifest.csv (in_tre==0, minus serverdata-covered,
minus rebuildable) — the census script already produces everything needed.

## Installer steps

1. Pick/create root; write nothing machine-specific into cfgs (they're
   relative + tracked). At most: record the base-TRE location if external.
2. git clone: SWG-Source/dsrc, SWG-Source/serverdata, missing-bits repo
   (pin shas; the pin-vs-latest choice applies to each).
3. Lay serverdata + missing bits into data\sku.0\sys.client\compiled\game.
4. Acquire base TREs (copy from an existing SWGSource install or download).
5. Rebuild compiled server/shared data from dsrc (TemplateCompiler,
   DataTableTool, CRC scripts; javac optional for script/) — see
   PAYLOAD-MANIFEST rebuild section.
6. Optional: pack texture payload into tools_texture.tre (TreeFileBuilder).
7. Pre-warm compiled_shader (or let first runs do it).
8. Run the smoke suite (_smoke-auto.ps1 already exists) as install
   verification.

## App/config changes this requires (tools side)

- Rework all tool cfgs to relative paths under the SOE-shaped root; move
  them into the tracked exe/win32 store. Kill the absolute
  `D:/SWG All Tools Working/...` and `D:\Code\...` references.
- Verify each tool's CWD assumption (launch from exe\win32; shortcuts set
  Start In). _smoke-auto.ps1 launches with correct cwd already.
- Re-scope QuestEditor's whole-root searchPaths.
- Nothing else code-side is known-needed; the exewin32 dialog is already
  removed and branch checks pass by construction.

## Install experience + launcher (proposed 2026-08-31)

Kenny's target UX: download one installer, run it, get coffee, come back to
a fully wired setup. Plus: a supplied "editor home page" app listing every
editor, launching them directly, with a sample use-case flow per editor for
learning.

### Two-stage install — the launcher IS stage 2

The heavy work (2.5 GB of git clones, TRE location, ~80k-file rebuild,
shader warm-up) is long-running network+CPU work that wants a real progress
UI, resume-on-failure, and error reporting. MSI custom actions are the
wrong tool for that. Split:

**Stage 1 — Inno Setup bootstrap installer** (small, signed, familiar
next-next-finish): installs the 16 editors + 10 CLI tools + the launcher
app + tracked cfgs into the SOE-shaped root, bundles MinGit (~50 MB, no
user-visible git dependency), creates shortcuts + uninstaller, then starts
the launcher in first-run mode.

**Stage 2 — the launcher's first-run wizard** does the long haul with
progress bars and a step checklist:
  1. Locate base TREs: "point me at your SWGSource client install"
     (folder picker + validation: count/verify the 209 .tre) or download
     if GR ever hosts them. Junction or cfg pointer into the root.
  2. git clone --depth 1 at pinned shas: dsrc, serverdata, missing-bits
     payload. (Resumable; retry per repo.)
  3. Lay serverdata + payload into data\sku.0\sys.client\compiled\game.
  4. Rebuild compiled server/shared data (TemplateCompiler, DataTableTool,
     CRC scripts). Skip javac/scripts by default.
  5. Warm compiled_shader (optional, "recommended" checkbox).
  6. Run _smoke-auto.ps1 headless; show the 16-row green/red scorecard as
     the finish screen. Things-just-work is PROVEN, not assumed.

Every step idempotent + a state file records completion, so a crash,
cancel, or POWER CUT resumes where it left off (each step re-runnable:
clone→fetch, copy→skip-if-present, compile→skip-if-output-newer). The same
wizard re-runs as "Repair / Update" from the launcher menu — update =
git pull the three repos at new pins + re-run rebuild, which makes the
launcher the update channel too.

### The launcher ("editor home page")

Small C#/.NET (WinForms or WPF + WebView2 for markdown) app in exe\win32:
- Tile grid: every editor (icon from the exe, name, one-line purpose,
  Launch). Launch sets cwd=exe\win32 and env (MIFF_CPP etc.) so editors
  always start correctly — the launcher owns the CWD contract.
- Per-editor page: sample use-case flow (open this sample file, make this
  edit, save to workspace) rendered from markdown in the repo. Content
  seeds already exist: docs/TOOLS-GUIDE.md Part 2.5 per-tool verdicts +
  traps, the handoff's per-editor knowledge, logs/_shots screenshots,
  known-good sample files (C:\save-test\swgcon-new.cnv, pt_campfire_s01.prt,
  the papercut-pass workspace pattern).
- Status strip: tree health (TREs found, repos at expected pins, last
  smoke result), Repair/Update button, god-client server address setting.
- Guides live as markdown in the repo -> corpus-tracked like everything
  else; the app just renders them.

Build note: launcher is a NEW small app — keep it out of the ancient
vcxproj web; a self-contained .NET 8 publish (single exe) drops into
exe\win32 with zero runtime prerequisites.

## Open questions

1. Base TREs: copy from user's existing SWGSource client vs host somewhere.
   7 GB-ish — probably "point at your client install" with a junction.
2. Texture payload: ship all 1 GB vs closure-trim first. (Closure walk still
   the pending analysis.)
3. Media 651 MB: drop unless a tool browses it (check SoundEditor defaults).
4. Pins: dsrc a05279872-vs-c7294da3e(+21), serverdata df41a07-vs-master.
   Leaning: take the pins swg-main uses, re-smoke once.
5. DECIDED 2026-08-31 (Kenny + analysis): missing-bits payload lives in a
   SEPARATE repo (Galaxies-Reborn org), wired into client-tools as a git
   SUBMODULE — the swg-main pattern. Rationale: Kenny wanted corpus changes
   tracked alongside the tools; the submodule pin gives that (payload repo
   history = pure data changelog; client-tools history shows every pin
   bump), while in-tree would put ~23k binary files / 1.6 GB into every
   code clone and grow this repo's history permanently. Installer clones
   the payload repo --depth 1 at install time at the pinned sha (audio +
   texture together in one repo; ~1.6 GB is fine, initial push is under
   GitHub's 2 GB pack limit).
6. Trial full-tree compile timing (validates step 5's duration estimate).
