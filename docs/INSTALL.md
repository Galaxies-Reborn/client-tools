# Installing the tools tree (prototype: `scripts/install.ps1`)

Builds the single-root SOE-shaped tools tree at `C:\swg\current` — TREs,
pinned data clones, all 16 editors on relative configs, the dsrc-compiled
server/shared data — and finishes by smoke-testing every editor.

This is the prototype of the launcher's first-run wizard. It copies apps and
TREs from local paths (the shipped installer will download/ship them); the
defaults are set for this machine, so no arguments are needed here.

## Prerequisites

- `git` and `python` on PATH (check: `git --version`, `python --version`)
- ~13 GB free on C:
- Internet access to github.com (three repo clones, ~2 GB total)

## Fresh install

1. If a previous tree exists and you want a truly fresh run, delete it first
   (skip this to keep/repair the existing tree instead):

   ```powershell
   Remove-Item -Recurse -Force C:\swg
   ```

2. Run the installer (from any directory):

   ```powershell
   powershell -NoProfile -ExecutionPolicy Bypass -File D:\Code\swg-qt-tools-worktree\scripts\install.ps1
   ```

3. Wait. Expected timings on this machine, ~55 minutes total:

   | phase | time |
   |---|---|
   | TRE copy | ~1 min |
   | dsrc + serverdata clones | ~3 min |
   | **legacy-tools-payload clone** | **up to ~30 min** (1 GB pack; GitHub's CDN is slow when cold) |
   | data lays, cfgs, materialize | ~4 min |
   | dsrc full-tree compile | ~10 min |
   | CRC tables | <1 min |
   | smoke (16 editors) | ~10 min |

   During the smoke, editor windows will open and close on their own — one
   at a time, about 30 seconds each. Don't click into them.

## What success looks like

- The last line is **`Install complete.`**
- The smoke prints a verdict per editor. Pass = **15 × `w` and exactly one
  `?` (UIBuilder)** — UIBuilder's main window is a dialog-class window the
  scorer refuses to guess about; it is working. Any other `?`, `c`, or `x`
  is a real finding.
- Scorecard also lands at `C:\swg\current\exe\win32\_smoke-results.csv`,
  per-editor logs under `C:\swg\current\exe\win32\logs\_smoke\`.
- Compile results: `C:\swg\current\exe\win32\_rebuild-report.txt`. The known
  acceptable failure count is **4** (dsrc sources with hyphens in their
  filenames — upstream data quirk, names covered elsewhere), listed in
  `_rebuild-failures.csv` beside it.

## If it stops partway (error, Ctrl+C, power cut)

Just run the same command again. Every completed step is recorded in
`C:\swg\current\_install-state.json` and prints as `[skip]` on the rerun;
work resumes at the first unfinished step. Copies and compiles inside a
step also skip anything already done, so reruns are cheap.

To force one step to redo, delete its line from `_install-state.json` and
rerun. To force everything, delete `C:\swg` and rerun.

## Known rough edges (prototype)

- The stage-B override corpus is copied from
  `D:\Code\Galaxies-Reborn\stage-B-override` — it has no repo home yet, so
  that directory must exist.
- Apps and TREs come from the D: dev tree and the local SWGSource client
  dir (`-AppSource` / `-TreSource` parameters if they ever move).
- A clean install intentionally lacks ~440 orphaned 2016-era files whose
  dsrc sources no longer exist; nothing in the tools needs them.
