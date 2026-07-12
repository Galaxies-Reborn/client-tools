# Pre-CU client restoration

This branch is an isolated client restoration line based on
`x64-dx9-vanilla`. It must not absorb the uncommitted DX11 renderer work from
the primary `client-tools` worktree.

## Current skills UI baseline

`SwgCuiSkills` already implements a substantial Pre-CU profession-tree view,
but its generated profession and skill-box headers came from an external
calculator checkout. That source is useful evidence, not a reproducible or
cleared build input.

The first restoration task is to establish deterministic provenance before
changing player-visible behavior. `scripts/generate_precu_skill_box_data.py`
can now generate or audit the displayed commands, schematics, and skill mods
from an explicit Pre-CU `skills.tab`.

Audit the current header without modifying it:

```powershell
python scripts/generate_precu_skill_box_data.py `
  --skills-tab <path-to-precu-skills.tab> `
  --audit-header src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiSkillBoxData.h
```

Generate a review candidate at a new path:

```powershell
python scripts/generate_precu_skill_box_data.py `
  --skills-tab <path-to-precu-skills.tab> `
  --output <candidate-output-path>
```

The tool refuses to overwrite an existing file unless `--force` is supplied.
Generated differences must be reviewed against the pinned Core3 snapshot,
client TRE evidence, and the umbrella repository's provenance policy before
replacing the checked-in header.
