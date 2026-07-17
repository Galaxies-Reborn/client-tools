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

Protocol v7 of the opt-in background bridge provides a narrowly bound live
gate for the two local fixture characters. `QueueCombatCanary` enqueues the
authentic Publish 14.1 `headShot1` command through the normal toolbar admission
contract and returns an atomic read of the actual combat queue. It accepts only
the fixture pair and a bounded repeat count of 1 through 16. `ClearCombatQueue`
uses the mediator's production clear action, while `CombatQueueStatus` is
read-only. The July 16 live run observed add counts of 4 and 3 on the two
clients, natural authoritative removal to zero, a sequence-specific clear of
`12 -> 1 -> 0` in 300 ms, and `6 -> 0` across a client relog. The server still
rejects `headShot1`; this gate validates the client queue lifecycle and does
not claim that Pre-CU command execution or HAM damage is implemented.

Example live probes against a loaded fixture client:

```powershell
.\scripts\Invoke-PrecuBackgroundInput.ps1 `
  -Action QueueCombatCanary -ClientProcessId <pid> -Repeat 8
.\scripts\Invoke-PrecuBackgroundInput.ps1 `
  -Action ClearCombatQueue -ClientProcessId <pid>
.\scripts\Invoke-PrecuBackgroundInput.ps1 `
  -Action CombatQueueStatus -ClientProcessId <pid>
```

Protocol v11 extends that identity-bound path for the first complete Marksman
tier-I matrix. `EquipCdefPistol` and `EquipCdefCarbine` locate only the fixture
weapons in the bound attacker's inventory and use the production inventory
equip request. `QueueBodyShot1` and `QueueLegShot1` then enter the same toolbar
admission and authoritative combat-queue path as the existing headShot1 canary;
the client never fabricates a hit, damage, HAM cost, or command result.

Run the source-contract and skill-data tests with:

```powershell
python -m unittest discover -s scripts/tests -p 'test_*.py' -v
```

## Publish 14.1 stat migration

`SwgCuiStatMigration` restores the retained `/pda.StatMigration` page against
its original nine-slider contract. The local character sheet exposes the
button only when examining the current player. On activation the mediator
loads racial slider bounds from `attribute_limits`, disables submission until
the server returns `StatMigrationTargetsMessage`, and preserves the exact
Health/Strength/Constitution/Action/Quickness/Stamina/Mind/Focus/Willpower
wire order.

Slider changes spend only the server-owned points-left balance. Submission is
blocked until that balance reaches zero and sends the legacy ten-integer
payload: nine targets followed by advisory points-left. The server remains
authoritative for every bound and the racial total; normal-world application
is intentionally deferred to the Image Designer transaction milestone.

## Skills lifecycle baseline

The skills mediator refreshes its profession tree, XP bars, skill-point total,
and skill-mod table from the retained player delta messages while the window is
open. Rebuilding a graph preserves the selected skill box and deduplicates its
dynamic widget registrations so frequent XP updates do not accumulate
callbacks or references.

Surrender is intentionally server-authoritative. The client first verifies
that the selected skill is learned, lists any learned transitive dependents,
and otherwise opens the retained typed confirmation dialog. The confirmation
callback revalidates ownership and dependencies, snapshots one command
sequence, and keeps the surrender button disabled until that queue entry
completes. Missing/invisible command rows fail closed; queue clears and scene or
character changes discard stale snapshots. Authoritative skill deltas refresh
the resulting UI state. The client never mutates skills, points, commands,
modifiers, or schematics locally.

## Skill data provenance

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
