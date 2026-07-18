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
rejected `headShot1` at that historical v7 milestone; the current v13
acceptance below supersedes that execution limitation.

Example live probes against a loaded fixture client:

```powershell
.\scripts\Invoke-PrecuBackgroundInput.ps1 `
  -Action QueueCombatCanary -ClientProcessId <pid> -Repeat 8
.\scripts\Invoke-PrecuBackgroundInput.ps1 `
  -Action ClearCombatQueue -ClientProcessId <pid>
.\scripts\Invoke-PrecuBackgroundInput.ps1 `
  -Action CombatQueueStatus -ClientProcessId <pid>
```

Protocol v13 extends that identity-bound path for the first complete Marksman
tier-I matrix. `EquipCdefRifle`, `EquipCdefPistol`, and `EquipCdefCarbine`
locate only fixture weapons in either bound player's inventory and use the
production inventory equip request. `QueueBodyShot1` and `QueueLegShot1` then
enter the same toolbar admission and authoritative combat-queue path as the
existing headShot1 canary. `CombatTimerStatus` reports only the latest
server-origin execute timer for those three commands, including its current and
maximum milliseconds. The client never derives a timer or fabricates a hit,
damage, HAM cost, defense roll, or command result.

The July 17 primary-accuracy run also exercised the v13 canary at a
62.99-meter surface distance with the compiled client command table declaring
`headShot1.maxRangeToTarget=64`. The real queue admitted one command, received
`Success`, captured the server-origin 4,725 ms execute timer, and applied real
Mind damage. Server fixture telemetry reported a 53.338253-percent Core3 hit
chance against the 53.32653-percent 63-meter model. The bridge supplied only
the fixed fixture target and production enqueue request; it did not bypass
client admission or create the result.

`QueueDurationControl` is a separate fixed control for the shared timing gate.
It queues only the authentic Publish 14.1 `headShot2` player command from the
bound attacker to the bound defender. Its fixed 1,500 ms execute timer comes
from the command table because the action is deliberately absent from
`precu_combat_overrides`. The command performs its normal standard-combat
action, while the identity-bound fixture owns and restores the resulting HAM,
target, PvP, and combat state. This proves that the Pre-CU weapon-speed
override remains fail-closed for commands not yet opted into the migrated
duration model.

Protocol v15 adds `QueueHealWound` for the first production medical-command
gate. The caller must supply the fixture patient OID explicitly; the bridge
then uses the ordinary `ClientCommandQueue::enqueueCommand("healWound", ...)`
path and atomically returns the actual local combat-queue count. It does not
create medicine, grant skills, choose a wound, modify HAM, bypass client
admission, or infer server success. The accepted lifecycle observed one
successful seven-second retained queue entry followed by an already-queued
cooldown rejection, with the server fixture independently proving exact Mind,
wound, charge, and medical-XP results.

```powershell
.\scripts\Invoke-PrecuBackgroundInput.ps1 `
  -Action QueueHealWound `
  -ClientProcessId <pid> `
  -TargetOid <fixture-patient-oid>
```

Protocol v16 adds `QueueHealDamage`. It accepts only an explicit patient OID
and queues the authentic five-second `healDamage` command through
`ClientCommandQueue`; server-side code remains solely responsible for medicine
selection, treatment recovery, HAM changes, cost, charges, and medical XP.

```powershell
.\scripts\Invoke-PrecuBackgroundInput.ps1 `
  -Action QueueHealDamage `
  -ClientProcessId <pid> `
  -TargetOid <fixture-patient-oid>
```

Protocol v17 adds `QueueTendDamage` and `QueueTendWound`. Both accept only an
explicit patient OID and admit the authentic five-second commands through
`ClientCommandQueue`. Tending is organic treatment: the bridge creates no
medicine and mutates no HAM, wounds, battle fatigue, or XP.

```powershell
.\scripts\Invoke-PrecuBackgroundInput.ps1 `
  -Action QueueTendDamage `
  -ClientProcessId <pid> `
  -TargetOid <fixture-patient-oid>

.\scripts\Invoke-PrecuBackgroundInput.ps1 `
  -Action QueueTendWound `
  -ClientProcessId <pid> `
  -TargetOid <fixture-patient-oid>
```

The accepted protocol-17 lifecycle admitted both commands at local queue count
one and later reported count zero. Server telemetry independently proved exact
Mind and secondary-wound costs, Health/Action damage treatment, Health-wound
treatment, and the asynchronous medical-XP result; the bridge remained queue
admission only.

Protocol v18 adds `QueueDiagnose`. It accepts only an explicit patient OID and
admits the authentic nonqueued command through the same toolbar path. The
server remains authoritative for six-meter organic-target validation and
creates the ten-entry medical SUI; the bridge does not inspect or mutate
patient state.

```powershell
.\scripts\Invoke-PrecuBackgroundInput.ps1 `
  -Action QueueDiagnose `
  -ClientProcessId <pid> `
  -TargetOid <fixture-patient-oid>
```

The accepted protocol-18 lifecycle admitted `diagnose` with local queue count
zero. Server telemetry observed SUI PID 518, all nine ordered wound values, and
Battle Fatigue as the tenth entry. Background Escape dismissed the page before
the server fixture restored its complete snapshot.

Protocol v19 adds `QueueMedicalForage`. It admits the authentic targetless,
nonqueued command without foreground focus. The bridge does not simulate the
search or select a reward: the server owns the adjusted Action cost, 8.5-second
stationary task, medical-foraging chance, area depletion, inventory check, and
food/resource/medicine-component result.

```powershell
.\scripts\Invoke-PrecuBackgroundInput.ps1 `
  -Action QueueMedicalForage `
  -ClientProcessId <pid>
```

Protocols v20 through v23 add the same narrow, off-focus admission boundary for
First Aid, incapacitated-player drag, Quick Heal, and Heal State. Protocol v24
adds `QueueCurePoison`. The caller supplies only the fixture patient OID and the
client enters the authentic nonqueued `curePoison` command through
`ClientCommandQueue`; the server remains authoritative for poison state,
organic-patient and PvP-help validation, antidote selection and power,
condition-treatment recovery, Mind cost, charge use, effects, and medical XP.

```powershell
.\scripts\Invoke-PrecuBackgroundInput.ps1 `
  -Action QueueCurePoison `
  -ClientProcessId <pid> `
  -TargetOid <fixture-patient-oid>
```

Protocol v26 retains the `QueueHealEnhance` action number and transports the
patient OID both as the normal queue target and as a narrow command parameter.
That compatibility path preserves an exterior fixture patient even when the
retained optional-target client row has not loaded it into the look-at object
map. The server resolves only an eligible living patient from that parameter
and continues to own hospital/camp/droid and combat admission,
enhancement-pack selection, battle-fatigue scaling, Focus-adjusted Mind,
charge use, buff replacement, medical XP, effects, and wound-treatment
recovery.

```powershell
.\scripts\Invoke-PrecuBackgroundInput.ps1 `
  -Action QueueHealEnhance `
  -ClientProcessId <pid> `
  -TargetOid <fixture-patient-oid>
```

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
