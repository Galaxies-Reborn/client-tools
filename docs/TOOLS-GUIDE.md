# SWG client tools — setup, usage, and an honest assessment

Covers the 16 editors that build out of this tree into
`src/build/win32/x64/Release/`.

**How this was produced.** Every claim about behaviour below comes from actually
launching the tool and driving it, or from reading the source that decides the
behaviour. Where something was inferred rather than observed, it says so. Where a
tool was only launch-smoked and never driven, it says that too — that distinction
is the whole point of this document, because a launch smoke is very nearly
worthless for these tools. Several of them boot perfectly and die on the first
file you open.

Screenshots referenced live in `src/build/win32/x64/Release/logs/_shots/`.
Drivers used: `scripts/_drive-generic.ps1`, `scripts/_drive-soundeditor.ps1`,
`scripts/_drive-animationeditor.ps1`, and `_smoke-auto.ps1` / `_shot2.ps1` in the
Release directory.

---

## Part 1 — the things that bite every tool

Read this before the per-tool sections. Most of the time lost on these tools is
lost to one of the following, and none of them announce themselves in the UI.

### 1.1 Which config file a tool actually reads

Do not assume `FooEditor.exe` reads `FooEditor.cfg`. Three of them do not.

| Tool | Config it actually loads | Source |
|---|---|---|
| SwgGodClient | `SwgGodClient.cfg` | `SwgGodClient/src/GameWidget.cpp:299` |
| **TerrainEditor** | **`tools.cfg`** | `TerrainEditor.cpp:490` |
| **UIBuilder** | **`uibuilder_searchpaths.cfg`** (its own FileLocator, not TreeFile; `defaults.cfg` is dead code) | `MainFrm.cpp:311,324` |
| ParticleEditor | `ParticleEditor.cfg` | `ParticleEditor/.../GameWidget.cpp:239` |
| AnimationEditor | `AnimationEditor.cfg` | `AnimationEditorGameWidget.cpp:18` |
| LightningEditor | `LightningEditor.cfg` | `LightningEditorGameWidget.cpp:18` |
| **SwooshEditor** | **`AnimationEditor.cfg`** | `SwooshEditorGameWidget.cpp:18` |
| NpcEditor | `NpcEditor.cfg` | `NpcEditor/.../GameWidget.cpp:160` |
| SoundEditor | `SoundEditor.cfg` | `SoundEditor.cpp:455` |
| ClientEffectEditor | `ClientEffectEditor.cfg` | `ClientEffectGameWidget.cpp:18` |
| QuestEditor | `QuestEditor.cfg` | `QuestEditor/MainWindow.cpp:137` |
| ShipComponentEditor | `ShipComponentEditor.cfg` | `ShipComponentEditor.cpp:319` |
| SwgConversationEditor | `SwgConversationEditor.cfg` + `.ini` | `SwgConversationEditor.cpp:188` |
| SwgDraftSchematicEditor | `SwgDraftSchematicEditor.cfg` + `.ini` | `Configuration.cpp:312,364` |
| SwgSpaceQuestEditor | `SwgSpaceQuestEditor.cfg` + `.ini` | `SwgSpaceQuestEditor.cpp:103` |
| SwgSpaceZoneEditor | `SwgSpaceZoneEditor.cfg` + `.ini` | `SwgSpaceZoneEditor.cpp:106` |

**`SwooshEditor.cfg` is tracked in the repo and is never read by anything.**
Editing it has no effect. Change `AnimationEditor.cfg` instead — and be aware
that doing so also changes AnimationEditor.

### 1.2 `maxSearchPriority=12` silently discards higher search paths

Every tool cfg here sets `maxSearchPriority=12`. The registration loop is

```cpp
int const maxPriority = ConfigFile::getKeyInt("SharedFile","maxSearchPriority",20);
for (int priority = 0; priority <= maxPriority; ++priority)      // TreeFile.cpp:117,133
```

so `searchPath13` and above are **never read**. No warning, no error — the key
simply does nothing. The library default is 20, which makes this easy to get
wrong. **If a search path appears to have no effect, check `maxSearchPriority`
first.** This cost real time once already.

Priorities normally in use: `searchTOC0-3`, `searchTree0,2-8`, `searchPath12`.
Free below the cap: 1, 9, 10, 11.

### 1.3 A search path that does not exist is a hard FATAL

```cpp
FATAL(!result, ("Could not convert to absolute path.  Does it exist?  %s", path));
```

`TreeFile_SearchNode.cpp:117`. Not a warning — the tool dies. This is why a cfg
pointing at a directory that only exists on one machine breaks a fresh checkout
outright rather than degrading.

Relative paths are legal and preferable: the constructor runs
`Os::getAbsolutePath()` against the current directory, which is the exe's own
directory. `searchPath11="../../exe/win32"` works and survives a clone.

### 1.4 Higher priority number wins

Nodes sort with `a->getPriority() > b->getPriority()` (`TreeFile.cpp:336`),
descending. So `searchPath12` overrides `searchTree8`, which overrides
`searchTree0`. Put an override tree high and a fallback low.

### 1.5 Some tools refuse any file outside a search path

`TreeFile::stripTreeFileSearchPathFromFile` maps an absolute path back to a
TreeFile-relative one. It `dynamic_cast`s each search node to `SearchPath` and
**ignores every TRE** (`TreeFile.cpp:1027`). So a file can be mounted and
readable and still fail to map.

Tools whose open handler hard-refuses on a failed map:

* **AnimationEditor** — `MainWindow.cpp:363`, warning only in `warning.log`
* **ClientEffectEditor** — `MainWindow.cpp:380`, same
* **ShipComponentEditor**, **TextureBuilder**, **TemplateEditor** — same call, in sub-dialogs

Tools that take any readable path: **SoundEditor**, **LightningEditor**,
**SwooshEditor**, **NpcEditor**, **QuestEditor**.

The symptom is brutal: you pick a file, the dialog closes, and **nothing
happens**. The reason is in `warning.log` and nowhere else.

### 1.6 The SOE loose data roots are the fix for most of this

`D:\SWG All Tools Working\swg\current` carries the original data loose on disk,
laid out exactly as the TREs name their entries. Two roots matter:

```
.../data/sku.0/sys.client/compiled/game     <- appearance/ clienteffect/ sound/ quest/ terrain/ ...
.../data/sku.0/sys.server/compiled/game     <- server datatables
```

Adding either as a `searchPath` (at 11 or below — see 1.2) makes everything under
it both loadable **and** mappable, which satisfies 1.5.

**The client/server split matters.** QuestEditor FATALs on
`datatables/item/master_item/master_item.iff`, which is a *server* datatable
present in **none** of the 209 client TREs. Client-only mounts can never satisfy it.

**Cost:** these are large trees. Mounting one takes a tool's startup from roughly
20s to 60s or more. Budget for it before assuming a hang.

Loose asset inventory in that tree:

| ext | count | ext | count | ext | count |
|---|---|---|---|---|---|
| `.iff` | 125112 | `.msh` | 24177 | `.sht` | 22945 |
| `.apt` | 6047 | `.sat` | 4924 | `.lmg` | 4882 |
| `.snd` | 7220 | `.prt` | 2101 | `.qst` | 1671 |
| `.cef` | 1045 | `.ilf` | 297 | `.ash`/`.lat` | 203 |
| `.swh` | 99 | `.trn` | 68 | `.ltn` | 49 |
| `.lay` | 46 | | | | |

### 1.7 The magenta sky is a config gap, not a renderer bug

Every Qt tool built on the shared `swgClientQtWidgets` GameWidget calls
`Game::install(Game::A_particleEditor)` (`GameWidget.cpp:384`). So
**ParticleEditor, AnimationEditor, SwooshEditor, LightningEditor and
ClientEffectEditor are all "the particle editor"** as far as `Game.cpp` is
concerned. All of them take the `setScene()` branch and all read the ground scene
from the **`[ParticleEditor]`** section of whatever cfg they loaded
(`ConfigClientGame.cpp:1088`).

```ini
[ParticleEditor]
	groundScene=terrain/tatooine.trn
```

A section named after the tool itself is **silently ignored** — do not add
`[LightningEditor]`. Without the key, the compiled default `terrain/simple.trn`
applies: a bare test terrain with no environment family, so the default
environment block's empty colour ramp takes a fallback that paints the sky
magenta `(255,0,255)`.

All five now carry the key. NpcEditor is `A_npcEditor` and deliberately gets no
scene at all — it uses its own backdrop instead.

### 1.8 `warning.log` is not a reliable witness

`src/build/win32/x64/Release/logs/warning.log` is overwritten by each tool run.
Some tools never write it at all — in a full 16-tool smoke only 11 produced one.
And some tools log user-facing errors **only to their own GUI panel**:
`SoundEditorUtility::report()` writes to an on-screen output window and never to
a file (`SoundEditorUtility.cpp:110`). **An empty log proves nothing.**

### 1.9 Legacy no-sku key names

Tools call `SetupSharedFile::install(false)` with no sku bits, so `TreeFile`
builds key names **without** the sku segment: `searchTree0`, not
`searchTree_00_0`. `client.cfg` carries only the `_00_` form. Copying `client.cfg`
over a tool cfg therefore mounts **zero** TREs — silently, because most tools
touch no TRE asset until the first file open. This was a real shipped bug in
SoundEditor (fixed, `d17043a48`).

### 1.10 Settings shared by every tool cfg

```ini
[SharedFile]
	maxSearchPriority=12
	asynchronousLoaderCallbackTimeBudgetMs=6
	TOCTreePath="D:/Code/SWGSource Client v3.0/"
[ClientGraphics]
	rasterMajor=11          ; DX11 backend
	windowed=true
	screenWidth=1280
	screenHeight=720
[SharedFoundation]
	ProductRegistryPath=Software/whitengold/Default
[SharedLog]
	logReportLogs=1
```

`rasterMajor=11` selects the DX11 renderer this port added. All tool cfgs mount
the same ~70 TREs from `D:/Code/SWGSource Client v3.0/` plus
`searchPath12="D:/Code/Galaxies-Reborn/stage-B-override"`.

---

## Part 2 — the tools

Status key: **driven** = opened a real asset and confirmed the result;
**booted** = reached its main window, nothing loaded.

| # | Tool | Status | Verdict |
|---|---|---|---|
| 1 | ParticleEditor | driven | works |
| 2 | AnimationEditor | driven | works, playback confirmed |
| 3 | SoundEditor | driven | works, playback confirmed |
| 4 | LightningEditor | driven | works |
| 5 | SwooshEditor | driven | works |
| 6 | ClientEffectEditor | driven | works after a cfg fix |
| 7 | NpcEditor | driven | works, best of the set |
| 8 | QuestEditor | driven | works after a cfg fix |
| 9 | ShipComponentEditor | driven | works |
| 10 | TerrainEditor | driven | works |
| 11 | UIBuilder | driven | works |
| 12 | SwgGodClient | driven as a client | connects and plays; editor features untested |
| 13 | SwgConversationEditor | driven (New) | tool works; no `.cnv` data exists |
| 14 | SwgDraftSchematicEditor | driven (boot data) | works |
| 15 | SwgSpaceQuestEditor | driven | works |
| 16 | SwgSpaceZoneEditor | driven | works |

---

### 1. ParticleEditor (`ParticleEditor_r.exe`)

Edits `.prt` particle effects — emitters, textures, colour/alpha/size ramps.

**Config:** `ParticleEditor.cfg`, including `[ParticleEditor] groundScene=terrain/tatooine.trn`
(see 1.7 — this is the section every Qt GameWidget tool reads).

**Setup:** works as shipped.

**Instructions:** `Ctrl+O`, pick a `.prt` — 2,101 loose under
`.../compiled/game/appearance/`, 1,769 more inside the TREs.

**Points of interest**

* Attribute tree per emitter; texture, colour, alpha, speed and size ramps.
* The 3D viewport renders against the Tatooine ground scene.

**Assessment: works.** This was the first tool ever driven past launch, and the
one that exposed the magenta-sky config gap that turned out to affect four other
editors. Only a single `.prt` load has been exercised; nothing edited or saved.

---

### 2. AnimationEditor (`AnimationEditor_r.exe`)

Edits `.ash` animation state hierarchies and `.lat` logical animation tables —
the state machine that decides which `.ans` animation plays for a logical action.

**Config:** `AnimationEditor.cfg`. Carries the `[ParticleEditor] groundScene` key
(1.7), which is also why SwooshEditor gets a working scene (1.1).

**Setup:** works as shipped for the route below. **The file dialog is a dead end**
— `openFile()` refuses anything not under a search path (1.5), and this cfg has
only `stage-B-override`. Do not waste time pointing it at the SOE tree.

**It does not open `.ans` files.** `.ans` are the compiled animations a LAT points
at, exactly as `.mp3` samples are what a `.snd` points at. Its documents are
`.ash`/`.lat` — 203 loose under `.../compiled/game/appearance/{ash,lat}/`.

**Instructions**

1. Launch and wait ~35s. **It loads the player's animation data by itself** —
   three tabs appear: `ASH - all_b.ash`, `LAT - all_m.lat`, `LAT - hum_m_face.lat`.
2. A few seconds later the viewport renders the player character on terrain.
3. **To play an animation: go to the `ASH - all_b.ash` tab, into the Actions
   sub-folder, and double-click an Action.** That is
   `ActionListItem::doDoubleClick()` calling
   `appearance->getAnimationResolver().playAction()` (`ActionListItem.cpp:122`).

**The LAT tabs cannot play anything.** `LogicalAnimationTableWidget` forwards
double-clicks to the base `ListItem::doDoubleClick()`, which is literally
`// Default: do nothing.` (`core/ListItem.cpp:213`). The `Logical Animation
Mapping` tree is open by default and looks like the obvious place to click — it
is a dead end for playback.

**Points of interest**

* File menu has **no accelerators at all**; navigate by arrows. Order:
  1 Open Shared Creature Template, 2 New Ash, 3 New Lat, 4 Open…,
  5 Open Target ASH(s), 6 Open target LAT(s), 7 Close, 8 Save (`Ctrl+S`).
* `Open Target ASH(s)/LAT(s)` act on `networkScene->getPlayer()`, so they need no
  dialog and no path mapping — the only reliable way in.
* Action items carry a blue **A** icon (dance_lyrical, dive, door_knock, duck,
  face_blow_kiss, …).

**Assessment: works, playback confirmed by hand.** Loading, rendering and
animation playback are all verified. Two honest caveats: the menu picks in my
scripted run were **no-ops** (the targets were already open, tab set unchanged),
and a scripted double-click could **not** be distinguished from the idle
breathing animation by frame differencing (0.02–0.15% changed pixels against a
0.06–0.07% idle floor). Automate this by targeting the Actions sub-folder
specifically and not by counting pixels. Nothing edited or saved.

---

### 3. SoundEditor (`SoundEditor_r.exe`)

Edits `.snd` sound templates — a template names a set of `.mp3`/`.wav` samples
plus playback rules (volume, pitch, loop, spatialisation).

**Config:** `SoundEditor.cfg`. No tool-specific section; it needs only the TRE
mounts, because samples resolve through TreeFile.

**Setup:** works as shipped. The `.snd` you open comes off the filesystem, so you
need loose files — the SOE tree has 7,220 under
`.../compiled/game/{sound,player_music/sound,voice/sound}`.

**Instructions**

1. `Ctrl+O` (File > Open). Native "Open Sound" dialog.
2. Type or pick a `.snd`, e.g.
   `D:\SWG All Tools Working\...\compiled\game\sound\music_combat_loop.snd`.
3. A Sound Template window opens. **Play** is bottom-right; **Restart** beside it.

**Points of interest**

* *Sample List* header shows count and total size — `28 samples @ 27174 KB`.
  Sizes come through TreeFile, so non-zero sizes prove your mounts are good.
* *Audio Debug* panel (Cache Miss / Cached Sample Count) is the best evidence the
  archives are actually being read.
* Play Order, Priority, Category (e.g. Background Music), Start Delay, Volume,
  Pitch, Loop Delay, Fade In/Out, Spatiality.

**Assessment: works, fully verified end to end.** Loading, sample resolution,
caching and audible playback all confirmed. The one trap is invisible: if the
mounts are wrong you get 28 lines of `Unable to find the sample in the tree file
path` **in the GUI output window only** — never in `warning.log`.

---

### 4. LightningEditor (`LightningEditor_r.exe`)

Edits `.ltn` lightning appearances — bolt geometry, chaos, amplitude, and the
shader plus start/end particle appearances.

**Config:** `LightningEditor.cfg`. **Needs the `[ParticleEditor] groundScene` key**
(added — see 1.7); without it the viewport is a magenta void.

**Setup:** works as shipped. Takes any readable path — no search-path mapping.

**Instructions**

1. `Ctrl+O`, "Open Lightning Effect".
2. Pick a `.ltn` — 49 loose in the SOE tree under `.../compiled/game/appearance/`,
   all named `force_lightning*`.
3. Parameters populate immediately.

**Points of interest**

* *Shader* with Frames (e.g. `4 (2x2)`) and FPS — an animated sprite sheet.
* *Large Bolts* / *Small Bolts* tabs: Thickness, Chaos, Base/Start/End Amplitude,
  Start Until, End From, Arc, Shader Stretch Distance, Shader Scroll Speed.
* *Start/End Appearance* — `.prt` particle effects, each Single or **Per Bolt**.
* *Debug*: Number of Bolts, End Points (Fixed Position), **Pause**.

**Assessment: works, with one unresolved caveat.** Loading is proven — two
different `.ltn` files produced correctly different shader and appearance paths
(`pt_force_lightning_anim.sht` vs `pt_force_lightning_dark_anim.sht`). It also
**persists its last-loaded effect across sessions** via QSettings, so a fresh
launch is not a blank slate; do not mistake restored state for a successful load.
*Caveat:* the 3D viewport looked identical for both effects. I could not confirm
the preview actually re-renders the selected bolt, and did not chase it.

---

### 5. SwooshEditor (`SwooshEditor_r.exe`)

Edits `.swh` swoosh trails — the ribbon effects on weapon swings and ship
contrails.

**Config: `AnimationEditor.cfg`, NOT `SwooshEditor.cfg`.** See 1.1. It gets a
correct ground scene purely because AnimationEditor.cfg happens to carry
`groundScene` — it has the best-looking viewport of the effect editors, by luck.

**Setup:** works as shipped. Any readable path.

**Instructions**

1. `Ctrl+O`, "Open Swoosh Effect".
2. Pick a `.swh` — 99 loose under `.../compiled/game/appearance/`, e.g.
   `pt_aerialstrike_ship_contrail_swoosh.swh`.

**Points of interest**

* *Shader*, Stretch Distance, Scroll Speed.
* Initial/Fade Alpha, **Multiply Color by Alpha (for additive shaders)**,
  Taper Geometry, Width, Samples Per Second, Number of Sample Positions
  (buckets), Spline Points Per Quad — which reports the resulting polygon count.
* *Debug*: **Show Final Game Swooshes**, an **Animation** dropdown (Punch, …) to
  swing the effect on a real character, Pause after each animation, Reference
  Swooshes and speed, visibility speed thresholds.
* Menus: File, **Options**.

**Assessment: works.** Loads cleanly and has the healthiest preview scene of the
effect editors — real terrain, sky and a character. The Debug animation dropdown
looks like the intended way to see a swoosh in motion; **not exercised.**

---

### 6. ClientEffectEditor (`ClientEffectEditor_r.exe`)

Edits `.cef` client effects — the composite "one event, many reactions" container
binding particle appearances, sounds, lights, camera shakes and force feedback.

**Config:** `ClientEffectEditor.cfg`. Needed **two** fixes:

* `[ParticleEditor] groundScene` (1.7) — was rendering magenta.
* `searchPath11` = SOE client data root (1.5/1.6) — **could not open any file at all.**

**Setup — this one does not work out of the box.** Its open handler refuses
anything it cannot map to a search path. Before the fix every `.cef` was rejected
with `not mappable to your TreeFile path`, logged to `warning.log` and **shown
nowhere in the UI** — the dialog just closes and nothing happens.

**Startup is now 60s+** because of the mounted tree. That is not a hang.

**Instructions**

1. `Ctrl+O` ("Load Client Effect"), dialog "Choose a file to open".
2. Pick a `.cef` — 1,045 loose under `.../compiled/game/clienteffect/`.
3. The tree fills with the effect's components.

**Points of interest**

* Tree: *Camera Shakes*, *Force Feedback Effects*, *Lights*, *Particle
  Appearances*, *Sounds*. `avatar_explosion_01.cef` yields Camera Shake 0,
  `appearance/pt_lg_explosion.prt` and `sound/exp_large_generic.snd`.
* Name/Value property columns; menus File, **Game**.

**Assessment: works, once configured.** The cfg gap made it look functional in a
launch smoke while being completely unusable in practice. Nothing was edited or
saved, and the *Game* menu was not explored.

---

### 7. NpcEditor (`NpcEditor_r.exe`)

Builds NPC appearances: pick a shared creature template, customise morphs and
wearables, then save server/shared `.tpf` templates plus a client `.mif`.

**Config:** `NpcEditor.cfg` — the most configured tool here.

```ini
[NpcEditor]
	backdrop=jedi
	defaultClientDsrc=".../dsrc/sku.0/sys.client/compiled/game/clientdata/npc"
	defaultClientRoot=clientdata
	defaultServerDsrc=".../dsrc/sku.0/sys.server/compiled/game/object/mobile"
	defaultServerRoot=object
	defaultSharedDsrc=".../dsrc/sku.0/sys.shared/compiled/game/object/mobile"
	defaultSharedRoot=object
	wearableFilterName=Armor:Leg(F)     ; Name/Expr are PARALLEL INDEXED LISTS —
	wearableFilterExpr=*leg*_f          ; each Name must be followed by its Expr
[ClientUserInterface]
	uiRootName=ui_root_npceditor.ui     ; SOE's own root; defines the /AvView page
```

It also needs `searchPath11="../../exe/win32"` so the tracked
`ui/ui_root_npceditor.ui` is found. Without that UI root the tool FATALs on the
missing `/AvView` page.

**Instructions**

1. `Ctrl+O`, "Open Shared Object Template".
2. Pick a `shared_*.iff` creature template, e.g.
   `.../compiled/game/object/creature/player/shared_human_male.iff`.
3. The avatar renders; the title becomes the resolved client data file, e.g.
   `Npc Editor (...) : client_shared_player_human_m.mif`.

**Points of interest**

* *Avatar Customization*: morph sliders — `blend_lipfullness_0/1`,
  `blend_chinsize_0/1`, `blend_fat`, `blend_ears_0/1`, `blend_noselength_0/1`,
  `blend_jaw_1`, and more below the fold.
* *Wearable Customization* panel; the toolbar wearable filter dropdown is driven
  by the `wearableFilterName`/`Expr` pairs above (shows `Armor:Leg(F)` / `*leg*_f`).
* Menus: File, View, Zoom, **Wearables**, Tools, **Backdrop** (`backdrop=jedi`).

**Assessment: the most complete tool in the set, and the most fragile.** It took
three stacked fixes to get here (a `strlen(NULL)` crash, the missing `/AvView`
page, an `append(NULL)`). It now renders a correct avatar on a proper backdrop
with working morph controls. Saving templates was **not** tested.

---

### 8. QuestEditor (`QuestEditor_r.exe`)

Edits `.qst` ground quests — task trees, signals, rewards.

**Config:** `QuestEditor.cfg`.

```ini
[QuestEditor]
	defaultQuestDirectory=".../data/sku.0/sys.client/compiled/game/quest/nym_themepark"
	defaultTaskDirectory=".../dsrc/sku.0/sys.shared/compiled/game/datatables/questtask/quest"
	defaultListDirectory=".../dsrc/sku.0/sys.shared/compiled/game/datatables/questlist/quest"
	defaultStfDirectory=".../data/sku.0/sys.client/compiled/game/string/en/quest/ground"
	serverObjectTemplateDirectory=".../data/sku.0/sys.server/compiled/game/object"
	stringPrefix=@quest/ground
	defaultBranch=current
```

`defaultQuestDirectory` sets the Open dialog's starting directory. The tool also
needs `QuestEditorConfig.xml` (tracked SOE original, `src/build/win32/exe/win32/`).

**Setup — did not work out of the box.** Opening any quest killed it:

```
FATAL: open 'datatables/item/master_item/master_item.iff' not found
ExceptionHandler invoked                    exit 0x80000003
```

That is a **server** datatable, absent from all 209 client TREs. Fixed by
mounting the SOE server root at `searchPath10` and the client root at `11`.
Startup is now slower; allow 60s+.

**Instructions**

1. `Ctrl+O`, "Choose a filename to open"; starts in `defaultQuestDirectory`.
2. Pick a `.qst` — 1,671 loose, e.g. `nym_themepark/u16_nym_meet_townspeople.qst`.
3. An MDI child opens, titled with the quest's **display name**.

**Points of interest**

* *Task Tree* — nested tasks with type and id, e.g.
  `Speak with the speaker for the Disgruntled Townspeople [Wait for Signal]
  [findTownsPeople]` → `Find the Water Treatment Technician [findFiltrationGuy]`
  → `[Immediately Complete Quest] [completeQuest]`.
* *Property/Value* pane; **QuestTask** / **QuestList** tabs; output pane at the bottom.
* Menus File, **Tool**, Window, Help. Blank component icons are expected —
  `data/internal/.../questeditor/image/` does not exist here. Cosmetic.

**Assessment: works, after a fix a launch smoke could never have found.** It
booted perfectly and died on the first quest opened. Loading and display are
verified; editing, saving and the Tool menu are not.

---

### 9. ShipComponentEditor (`ShipComponentEditor_r.exe`)

Edits space ship chassis and component descriptors, and maintains a derived
database.

**Config:** `ShipComponentEditor.cfg`. No tool-specific section; its paths were
ported from the SOE tree in an earlier session.

**Setup:** works as shipped. **No file open needed** — it loads data at boot.

**Instructions**

1. Launch. The Chassis Editor grid is populated within ~25s.
2. Use *Name Filter* to narrow the list; switch tabs for the other views.
3. **Reload Templates** / **Regenerate DB** rebuild from source templates.

**Points of interest**

* Tabs: **Chassis Editor**, **Component Descriptors**, **Attachments by Chassis**.
* Grid columns: Name, Flyby Sound (`sound/eng_flyby_awing`), HitSoundGroup,
  WingSpeed, reactor (`rct_0`, `rct_basic`), HitWeight, Target, engine.
* Real data: `arc170` and tiers 1–5, `awing` and tiers 1–10, `awing_mercenary`,
  `assaultship`, `basic_tiefighter`, `basic_z95`, `blacksun_heavy_*`,
  `blacksun_light_*`.
* Left pane: **Templates** / **Orphaned Templates** — the orphan list is worth a look.

**Assessment: works, and is the least fussy tool here.** Boots straight into real
data with no setup at all. Only the Chassis Editor tab was examined; the other two
tabs, the sub-dialogs (which *do* use search-path mapping and may hit 1.5), and
Regenerate DB are untested.

---

### 10. TerrainEditor (`TerrainEditor_r.exe`)

The planet terrain editor — heightmaps, shader/flora/radial families,
environments, water, roads, and the layered rule tree that generates a world.
The deepest tool in the set.

**Config: `tools.cfg`, not `TerrainEditor.cfg`** (`TerrainEditor.cpp:490`). No
extra search paths are needed — the stock TRE mounts are enough to open a planet.

`TerrainEditor.ini` is **not** a config file: it is the shader/family list data
(~229 KB of `SF/dirt/dirt_bigcracks=…`). Do not look for settings in it.

**The MFC profile is in the registry, at a path you will not guess:**

```
HKCU\Software\Software\Sony Online Entertainment\TerrainEditor\terrainEditor\
```

The doubled `Software` is real. `SetRegistryKey("Software\Sony Online
Entertainment\TerrainEditor")` (`TerrainEditor.cpp:331`) passes a full path where
MFC expects a company name, so MFC prefixes `Software\` and appends the app name
`terrainEditor`. Sections under it: `Tip`, `Settings`, `Recent File List`.

**Tip of the Day.** The dialog is *modal at startup and will block any scripted
input.* It appears because the tips file `terrainEditor.tip` does not exist — and
it never did: there is no `.tip` file anywhere in the SOE reference tree either,
so SOE's own build showed the same dialog. What you see is the failure branch,
string `CG_IDS_FILE_ABSENT` = *"Tips file does not exist in the prescribed
directory"* (`TerrainEditor.rc:2520`). It is loaded by a plain `fopen` beside the
exe (`TipDialog.cpp:39`), so no cfg or TRE can supply it.

To suppress it permanently, either untick "Show tips on startup" once, or set:

```
HKCU\Software\Software\Sony Online Entertainment\TerrainEditor\terrainEditor\Tip
    StartUp (DWORD) = 1
```

(`m_bStartup = !GetProfileInt("Tip","StartUp",0)`, so 1 means "do not show".)
Verified: the dialog stops appearing.

**Instructions**

1. Launch. Dismiss/suppress Tip of the Day.
2. **File > Open** (`Ctrl+O` is bound to `ID_FILE_OPEN` but proved unreliable to
   script; the `Alt+F`, `O` menu route works).
3. Pick a `.trn` — 68 loose under `.../compiled/game/terrain/`, e.g.
   `tatooine.trn`. Title becomes `terrainEditor - tatooine.trn`.

Note **there is no command-line file open**: `ProcessShellCommand` is commented
out with `//-- don't open a file by default!` (`TerrainEditor.cpp:467`).

**Points of interest**

* **2D Map** with its own toolbar — Height, V Color, Shader, Flora, Radial, Env,
  pass, Water, Light, Profile, Grid, Center, Zoom; Boundary
  Rectangle/Circle/Polygon/Polyline; Modify Polygon/Polyline; Affector
  Road/Ribbon/Filter; L/M/H detail.
* **Construction Layers** — the real content. For Tatooine: `Base_Height_Rules`
  (AffectorHeightFractal, AffectorShaderConstant, Base_ColorRampFractal,
  AffectorEnvironment), `Large color`, `Global Grass` (FilterSlope,
  AffectorRadialNearConstant), `Hills` (BoundaryCircle, FilterFractal,
  AffectorHeightFractal), `Canyons-on plains92`, `Erosion trenches`, …
* **Shader / Flora / Radial Families**, **Bitmaps**
  (`moseisely_road_height`, `tatt_heightmap_jawa_tradepost`), **Environments**
  (anchorhead, arch_valley, bestinetownship, bf_canyon_maze, bf_dune_sea,
  bf_oasis, bf_ridge, escapepod).
* **Properties** panel showing the selected environment's Family Data colour and
  Feather Clamp.
* Menus include a **Debug** menu; toolbar has 3D View, Console, Warnings, Profile.

#### Baking — Tools > Bake Terrain / Bake Rivers-Roads / Bake Flora

This is the payload of the tool, and **Bake Flora is not in-process**: it shells
out to **`Turf_r.exe`** (`TerrainEditorDoc.cpp:1761` -> `_bakeFlora`), swapping the
`.trn` extension for `.tcf`, and pops *"Could not find Turf_r.exe!"* if the exe is
missing. It was missing — Turf was in the solution but had never been ported to
x64 (no `OutDir`/`TargetName`, and a link line still pointing at x86 libs), and it
read `client.cfg`, whose sku-keyed form mounts zero TREs. Both fixed; see the
`Turf` commit. **`Turf_r.exe` must be present in the Release directory.**

How the flora bake actually works, which is not obvious from the UI:

* the `.tcf` is the **output**, not an input — none ship with the game
* the planet is loaded as `terrain\<planet>.trn` **through TreeFile**, i.e. from
  the mounted TREs, *not* from the `.trn` you have open on disk
* the planet name is taken from the **`.tcf` filename**, so it must be named for
  the planet
* the default clip is the whole world (±16384); `/R x0,z0,x1,z1` bounds it

Driving Turf directly is far quicker than going through the UI, and does not
write into your data tree:

```
Turf_r.exe <out>	atooine.tcf /R3200,-5100,3700,-4600 /f
```

**Verified 2026-08-24.** A 500m box near Mos Eisley sampled 59 flora and wrote a
6,248-byte `.tcf` whose header parses correctly: type 8, version 1, terrainName
`terrain	atooine.trn`, tile width 16m, bounds X 712..744 / Z 193..225,
`numberOfFloraSampled` = 59 matching the log, height range 0.000..51.844, and a
payload of exactly 32x32x6 = 6144 bytes. `numberOfFloraSampled` is only patched
back at the end of the writer, so the file is complete.

**Residual defect — Turf crashes AFTER writing.** `"Finished sampling"` never
prints; the log stops after the sampling loop and the process dies with
`FATAL: ExceptionHandler invoked`, exit `0x80000003`. The output is already on
disk and correct by then, so the crash is in the teardown that follows the write:

```cpp
samplerAppearanceTemplate->writeStaticCollidableFloraFile(o_flora_file, tileBounds);
delete appearance;
delete appearanceTemplate;   // log ends here
```

(`HeightSampler.cpp:1236-1240`.) Practical impact: the baked file is good, but the
non-zero exit means TerrainEditor's `ProcessSpawner` sees a failed child and may
report the bake as failed. Not yet diagnosed — a double-free between the
appearance and its template is the obvious suspect, but that is untested.

**Bake Terrain and Bake Rivers/Roads are in-process** (`mapFrame->bakeTerrain()`,
`mapFrame->updateRiversAndRoads()`) and were **not** exercised.

**Assessment: works, and is impressive.** It opens real planet data with the full
rule tree intact and needs no config beyond stock. Only loading was exercised —
no editing, no 3D View, no save, and the generation/preview paths are untouched.
The Tip dialog is the single biggest practical annoyance and is now solvable.

---

### 11. UIBuilder (`UIBuilder.exe`)

Edits the client UI layout — the `.ui` "User Interface Workspace" files that
define every in-game page and widget.

**Config: neither a `.cfg` in the TreeFile sense nor `defaults.cfg`.**
UIBuilder does **not use TreeFile at all**; it has its own `FileLocator`.

* `defaults.cfg` — **dead**. Its loader at `MainFrm.cpp:311` is commented out
  inside a `/* TODO */` block. Creating one has no effect.
* `uibuilder_searchpaths.cfg` — **live** (`MainFrm.cpp:324`). Plain text, one
  directory per line, each handed to `FileLocator::addPath()`. `./` is always
  added first. It did not exist here; one has been created listing the SOE client
  data root, its `ui/` subdirectory, and `stage-B-override`.

**Instructions**

1. Launch — a narrow dialog-shaped window (it is a dialog-based MFC app, so its
   `#32770` class **is** the main window; smoke scripts mis-score this as `?`).
2. `Ctrl+O` → "Select a User Interface Workspace to Open".
3. Pick a `.ui` — only 5 exist in the SOE tree, under `.../compiled/game/ui/`:
   `ui_root.ui`, `ui_clienttools.ui`, `ui_root_npceditor.ui`, `ui_script.ui`,
   `ui_test.ui`. Note the file filter is `ui_*.ui`, so files must start with `ui_`.

**Points of interest**

* *Objects* tree with Back/Forward/Up navigation. `ui_root.ui` yields the whole
  client UI: GroundHUD, HudSpace, Login, PlanetMap, Console, Warnings, Debug,
  Auction, Craft, Skill, Mission, GameMenu, Controls, SpaceLoading,
  AvProfessionTemplateSelect, AvSimple, TextStyleManager, …
* Property pane per object — DragAccepts, BackgroundColor, BackgroundOpacity,
  BackgroundTint, DragGoodCursor/DragBadCursor, ContextCapable, …
* *Performance Diagnostics*: FPS with a Limit toggle, Flushes, Triangles.
* This is the format `ui_root_npceditor.ui` is in — the file NpcEditor needs for
  its `/AvView` page.

**Assessment: works.** Opened `ui_root.ui` and got the complete object tree and
live properties. It is also the tool most likely to be misjudged: its main window
is a `#32770` dialog, so automated smoke scoring flags it as suspicious when it
is perfectly healthy. Nothing was edited or saved.

---

### 12. SwgGodClient (`SwgGodClient_r.exe`)

The in-world editor: a **full game client** with editor docking panels bolted on.
Used to place and edit objects in a live world.

**Config:** `SwgGodClient.cfg`. No tool-specific section.

**Setup — it needs a running game server, and it needs the INDEXED login keys.**

`SwgGodClient.cfg` shipped with no `[ClientGame]` section at all, so it fell back
to `localhost`. Point it at your server with:

```ini
[ClientGame]
	loginServerAddress0=192.168.1.200
	loginServerPort0=44453
```

**The index is mandatory.** `SwgCuiLoginScreen.cpp:284-303` walks
`loginServerAddress0`, `loginServerPort0`, `loginServerAddress1`, … and stops at
the first missing address; an entry is accepted only if its port is also present
and non-zero. The unindexed `loginServerAddress` in `ConfigClientGame.cpp:991`
(default `localhost`, port 44453) is a **different key that the login page never
reads** — setting only that one looks correct and does nothing. This is the single
most likely hour to lose on this tool.

`autoConnectToLoginServer` defaults false, so credentials are typed at the LOG IN
box. Setting `loginClientID` / `loginClientPassword` / `autoConnectToLoginServer=true`
would skip it, at the cost of putting credentials in a tracked file.

Confirm it worked from `warning.log`:

```
Login: assembled 1 address(es) from ClientGame/loginServerAddress<N>.
Login: connecting to 192.168.1.200:44453
Login: OUTCOME -- connection to the login server OPENED.
```

**Points of interest** (all visible, none exercised)

* Docking panels: *Objects / Scripts / ServerTemplates*, *Attributes / Scripts /
  ObjVars*, *Brushes / Palettes*, a *Bookmark* tree (Camera Bookmarks, Object
  Bookmarks), and a Name/Value tree rooted at `root`.
* Menus: File, Edit, View, Game, **Script**, **ObjectTemplate**, Tools, Window, Help.
* Status bar: Game / God Client Focus toggles, Distance readout,
  `Buildout Region: <unknown>`, `Position: (0.0, 0.0, 0.0)`.
* Its file dialogs deal in `.tpf` (server object templates), `poi*.lay` (terrain
  layers), `.mif` (client data) and `.ilf` (interior layouts) —
  `ActionsEdit.cpp`, `ActionsGame.cpp`.

**Assessment: works as a client. The editor half is still untested.**

Verified against a live server (2026-08-24): authenticated as `swg`, connected to
`192.168.1.200:44453`, reached character selection, and entered the world.
`WorldSnapshot` streamed cleanly while moving — `loaded` climbed 68 -> 170 with
**`refused=0` on every tick** and no FATAL, no assert, no disconnect. So the whole
client-side path works: authentication, cluster and character selection, world
entry, proximity streaming, and the DX11 renderer drawing a live scene.

**Be precise about what that does NOT cover.** Everything that makes this a *god*
client is still unexercised:

* object selection and the *Objects* / *ServerTemplates* panels against live objects
* the **ObjectTemplate** and **Script** menus
* *Brushes* / *Palettes*, and the *Bookmark* tree
* the file dialogs — `.tpf` server object templates, `poi*.lay` terrain layers,
  `.mif` client data, `.ilf` interior layouts
* creating, moving, editing or saving anything in the world

Treat "SwgGodClient works" as meaning *it connects and plays*. Its editing
features remain the largest untested surface of any tool here.
---

### 13. SwgConversationEditor (`SwgConversationEditor_r.exe`)

Edits NPC conversation trees.

**Config:** `SwgConversationEditor.cfg` **and** `SwgConversationEditor.ini`
(both required; the `.ini` is a tracked SOE original).

```ini
[SwgConversationEditor]
	scriptPath=".../dsrc/sku.0/sys.server/compiled/game/script/conversation"
	stringPath=".../data/sku.0/sys.client/compiled/game/string/en/conversation"
```

**Setup — there is nothing here for it to open.** Its document type is `.cnv`
(`SwgConversationEditor Files (*.cnv)`, `IDR_SWGCONTYPE`) and **there is not a
single `.cnv` file anywhere on this machine.** What shipped is the *output*:
1,363 `.java` conversation scripts under the configured `scriptPath`. `.cnv` was
SOE's authoring format and stayed in their source control.

There is a ScriptLib import path (`ScriptTreeView.cpp:1115`, filter
`ScriptLib Files *.java`) and a text import/export (`*.txt`), which are the only
plausible ways to get data in. **Neither was tested.**

**Startup dialogs** — both cosmetic, click OK:

1. *"not running from `<branch>\exe\win32`"* — checks whether the CWD contains
   the substring `exewin32` after stripping slashes. Our CWD is
   `…/x64/Release`, so it can never match. Structural.
2. *"running out of the 'qt-tools-worktree' branch which does not match the
   directories specified by …cfg"* — compares the branch extracted from the CWD
   against the branch in the configured paths (`current`). Structural.

**Assessment: the TOOL works. Only the documents are missing.** `File > New`
(`Ctrl+N`) creates `swgcon1` and gives the complete editor:

* **Conversation Editor** pane — tree seeded with `Root` -> `Npc Branch`, and a
  toolbar of Move Up/Down, **Add Branch**, **Add Response**, **Find Text**,
  **Test Branch**.
* **Script Editor** pane — Conditions, Actions, **%TO / %DI / %DF Tokens**,
  **Libraries** (`ai_lib`, `chat`, `conversation`, `utils`), Labels, Triggers,
  with Add Condition/Action/Label/Library and Condition/Action **Wizards**.
* Main toolbar adds **Spell Check**, **Scan**, **Compile Debug**, **Compile
  Release**.

The four library names correspond to real files under
`dsrc/.../script/library/{ai_lib,chat,conversation,utils}.java`. Be precise about
what that proves: the tree is populated from the new conversation's own default
`LibrarySet` (`ScriptTreeView.cpp:464`), so it is *consistent with* a correct
`scriptPath` rather than proof that it resolved.

So this is not a broken tool — it is a working authoring environment with nothing
authored to open. Creating and compiling a conversation from scratch is possible;
neither was carried through, and nothing was saved.
---

### 14. SwgDraftSchematicEditor (`SwgDraftSchematicEditor.exe`)

Edits crafting draft schematics.

**Config:** `SwgDraftSchematicEditor.cfg` **and** `.ini` (both tracked SOE originals).

```ini
[SwgDraftSchematicEditor]
	serverObjectTemplatePath=".../dsrc/sku.0/sys.server/compiled/game/object/"
	draftSchematicDirectory=draft_schematic
```

**Setup:** works as shipped, and **loads its data at boot** — no file open needed.

**Points of interest**

* Tree of 24 categories under `draft_schematic`: armor, bio_engineer, camp,
  chemistry, clothing, community_crafting, dance_prop, droid, food, furniture,
  genetic_engineering, instrument, item, munition, ranger, reverse_engineering,
  scout, slicing, space, spices, structure, test, vehicle, weapon.
* Output pane confirms the parsed tables: `4 armorRatings` (default
  `AR_armorNone`), `29 craftingTypes` (`CT_genericItem`), `13 damageTypes`
  (`DT_kinetic`), `6 ingredientTypes` (`IT_none`), **`749 resourceTypes`**,
  `13 stringTables`, `61 xpTypes` (`XP_crafting`).
* Tabs: Output, **Shell**, Old/New server `.tpf`, Old/New shared `.tpf` — it
  generates template source, which is the actual point of the tool.
* Toolbar includes **Scan**, **Compile** and **p4 edit**. The Perforce button is
  dead weight here — there is no Perforce.

**Assessment: works, boots straight into real data.** The parse counts match what
was seen in earlier sessions, so the data path is stable. Only the boot state was
observed — no schematic was opened, edited, scanned or compiled, and the `.tpf`
generation tabs are untested.

---

### 15. SwgSpaceQuestEditor (`SwgSpaceQuestEditor_r.exe`)

Edits space mission/quest definitions.

**Config:** `SwgSpaceQuestEditor.cfg` **and** `.ini`. The `.ini` is read through
TreeFile (`Configuration.cpp:1430`), not plain `fopen` — unusual for these MFC
tools, and it means the `.ini` must be reachable via the search paths.

```ini
[SwgSpaceQuestEditor]
	serverMissionDataTablePath=".../dsrc/sku.0/sys.server/compiled/game/datatables"
	sharedStringFilePath=".../data/sku.0/sys.client/compiled/game/string/en"
	spaceQuestDirectory=spacequest
	spaceZoneDataTablePath=".../data/sku.0/sys.server/compiled/game/datatables/space_zones/buildout"
```

The `.ini` supplies `[SpaceZones]`, `[QuestCategories]` and
`[MissionTemplateTypes]`; each has its own "could not find any…" message box if
missing (`Configuration.cpp:1538,1561,1585`).

**Startup dialogs:** the same two structural warnings as SwgConversationEditor.
A **second dialog is permanent and correct** — it is the tool reporting real
configuration state, not a failure.

**Setup/state:** it reaches a full MFC frame and has previously been seen with
`naboo_imperial_4.tab` loaded in its title, and 20 mission categories parsed — so
its data path works. In this pass the modal warnings were dismissed but **no
document was opened.**

**Assessment: works.** The document-type resource declares no file extension,
which is why the open route is not obvious — **but you do not use File > Open.**
The tool boots with the whole `spacequest` tree already loaded (20 categories:
_debug, assassinate, convoy, delivery, delivery_no_pickup, destroy, destroy_duty,
destroy_surpriseattack, escort, escort_duty, inspect, patrol, recovery,
recovery_duty, rescue, rescue_duty, salvage, space_battle, space_mining_destroy,
survival) and the *Configuration* tab already parsed.

**Expand a category and double-click a `.tab` leaf.** Title becomes
`SwgSpaceQuestEditor - [<name>.tab]` and you get a full property editor:

* *Property/Value* grid — Mission Template, `PT_cargo`, `PT_navPoint` /
  `PT_navPointList`, `PT_questName`, `PT_spaceFaction`, `PT_spaceMobile` /
  `PT_spaceMobileList`, `PT_spaceZone`, `PT_spawner` / `PT_spawnerList`.
* *StringId/Text* grid and a *Quest Log Data* grid
  (`CATEGORY = @spacequest\quests:neutral_category`).
* Bottom tabs: **Configuration**, Output, Warning, **Shell**.

Verified on `_debug.tab`. Note that file legitimately contains
`PT_notImplemented = ERROR(2): see asommers` — that is SOE test content, not a
tool failure. Nothing was edited or saved.
---

### 16. SwgSpaceZoneEditor (`SwgSpaceZoneEditor_r.exe`)

Lays out space zones — spawns, navigation, paths.

**Config:** `SwgSpaceZoneEditor.cfg` **and** `.ini` (also read via TreeFile).

```ini
[SwgSpaceZoneEditor]
	spaceMobileDataTable=".../data/sku.0/sys.server/compiled/game/datatables/space_mobile/space_mobile.iff"
	squadDataTable=".../data/sku.0/sys.server/compiled/game/datatables/space_content/spawners/squads.iff"
```

Document type is `SwgSpaceZoneEditor Files (*.tab)`.

**Startup dialogs:** the same two structural warnings, verbatim — *"running out
of the 'qt-tools-worktree' branch which does not match the directories specified
by SwgSpaceZoneEditor.cfg"*. Source: `SwgSpaceZoneEditor.cpp:127,133,140`. Both
cosmetic.

**Points of interest**

* Toolbar: New, Open, Save; **XZ / XY / ZY** view-plane switches; **Hide Nav**,
  **Hide Spawn**, **Hide Misc**, **Hide Paths**, **Hide Grid**; **Info**,
  **Validate**, **P4 Edit**.
* Status bar carries a live `<0, 0, 0>` coordinate readout.
* Menus: File, View, Help.

**Assessment: works.** The `.tab` / `.iff` confusion is resolved: **open the
`.tab` SOURCE under `dsrc/`, not the compiled `.iff` the cfg points at.**
`OnOpenDocument` calls `loadFromSpreadsheet()` on the `.tab` and swaps the
extension to `.iff` itself when it needs the compiled form
(`SwgSpaceZoneEditorDoc.cpp:392,859`).

There are **31** of them under
`dsrc/sku.0/sys.server/compiled/game/datatables/space_zones/buildout/`
(`space_tatooine.tab`, `space_corellia.tab`, `e3_space_*.tab`, ...).

Verified with `space_tatooine.tab`: title becomes
`SwgSpaceZoneEditor - [space_tatooine.tab]`, the tree fills with **Nav Points**,
**Spawners** and **Miscellaneous**, and the zone view plots the objects across a
ruled XZ grid (roughly 100-1300 by 100-800) with +X/-X/+Z/-Z axis labels. The
XZ/XY/ZY buttons switch projection and the Hide Nav/Spawn/Misc/Paths/Grid
toggles filter the view. Nothing was edited or saved.
---

## Part 3 — what is still not known

Being explicit, because the gap between "launches" and "works" is where all the
risk in this tree lives.

* **Nothing has ever been saved by any tool.** Every result above is a read path.
  Save/export is completely untested across all 16.
* **Nothing has been edited.** No parameter changed, no object created.
* **15 of 16 are now driven past launch.** The three that looked broken were not:
  SwgSpaceZoneEditor needed the `.tab` source under `dsrc/` rather than the
  compiled `.iff`; SwgSpaceQuestEditor is a tree-browser, not a File>Open tool;
  and SwgConversationEditor works via `File > New` — only its `.cnv` documents
  are absent from this machine.
* **SwgGodClient cannot be assessed without a game server.**
* **LightningEditor's 3D preview** may not re-render the loaded effect; the
  viewport looked identical for two different `.ltn` files.
* **SwooshEditor's Debug > Animation** dropdown (the intended way to see a swoosh
  in motion) was not used.
* **ShipComponentEditor's** other two tabs, its sub-dialogs (which use the
  search-path mapping of 1.5 and may well fail the same way ClientEffectEditor
  did) and Regenerate DB are untested.
* **QuestEditor's Tool menu**, and **ClientEffectEditor's Game menu**, unexplored.

### Config changes made during this pass

| File | Change | Why |
|---|---|---|
| `NpcEditor.cfg` | `searchPath11="../../exe/win32"` | reach the tracked UI root; relative so it survives a clone |
| `LightningEditor.cfg` | added `[ParticleEditor] groundScene` | magenta sky |
| `ClientEffectEditor.cfg` | added `[ParticleEditor] groundScene` | magenta sky |
| `ClientEffectEditor.cfg` | `searchPath11` = SOE client root | could not open any file |
| `QuestEditor.cfg` | `searchPath10` = SOE **server** root, `searchPath11` = client root | FATAL on every quest open |
| `uibuilder_searchpaths.cfg` | created | UIBuilder had no search paths at all |

Registry (not in the repo, per-machine):
`HKCU\Software\Software\Sony Online Entertainment\TerrainEditor\terrainEditor\Tip\StartUp = 1`
to stop the Tip of the Day dialog blocking scripted input.
