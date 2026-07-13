# Pre-CU client restoration

This is the isolated `x64-dx9` restoration line in
`swgsais/pre-cu-reborn-tools`, seeded from the `reborn-master` client-tools
tree. It must not absorb unrelated DX11 renderer work from other worktrees.

## Combat queue baseline

The first queue slice restores the direct HUD-owned `SwgCuiCombatQueue`
mediator against the retail `/GroundHUD.CombatQueue` contract. It observes
queue additions/removals and player combat-state changes for the mediator's
full lifetime, renders only commands marked `addToCombatQueue`, and uses the
combat target for the target label.

The clear path intentionally enumerates combat entries and removes each by
sequence ID. `ClientCommandQueue::clear()` remains the legacy broad queue
operation and is not used by the combat-queue UI, so non-combat commands are
preserved. Local mutation waits for the server's authoritative removal reply,
which also preserves the executing front command and its real wait/status
payload. Removal callbacks copy only the sequence ID and wait time needed by
the fade timer; they never retain the queue entry pointer after the callback.

Run the source-contract and skill-data tests with:

```powershell
python -m unittest discover -s scripts/tests -p 'test_*.py' -v
```

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
