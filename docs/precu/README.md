# Pre-CU client restoration

This is the isolated `x64-dx9` restoration line in
`swgsais/pre-cu-reborn-tools`, seeded from the `reborn-master` client-tools
tree. It must not absorb unrelated DX11 renderer work from other worktrees.

Protocol v91 closes the first generated Core3 combat pair. The bridge can
equip the identity-bound fixture staff, unequip to the replicated default
unarmed weapon, and enqueue `polearmLegHit1` or `unarmedHeadHit1` through the
ordinary production command queue. Read-only weapon-status actions expose
whether each command was loaded, the replicated weapon type, valid/invalid
masks, and the result of the same `WeaponObject::weaponTypeSatisfies` check
used by toolbar admission.

The live gate proved staff type 7 against `0x0080` and unarmed type 6 against
`0x0040`. It also exposed an older loose `command_table.iff` shadowing the
new compatibility TRE; the assets staging script now synchronizes both
generated IFFs into that higher-precedence loose overlay.

`Capture-PrecuClientWindow.ps1` uses `PrintWindow` against one explicit client
PID, so visual evidence can be captured without moving the cursor or taking
foreground focus.

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

Protocol v27 adds `QueueExtinguishFire`. The caller supplies only the fixture
patient OID and the client admits the authentic nonqueued `extinguishFire`
command through `ClientCommandQueue`. The server remains authoritative for
fire state, organic-patient and PvP-help validation, blanket selection and
power, shared treatment recovery, Focus-adjusted Mind cost, charge use,
effects, and medical XP.

```powershell
.\scripts\Invoke-PrecuBackgroundInput.ps1 `
  -Action QueueExtinguishFire `
  -ClientProcessId <pid> `
  -TargetOid <fixture-patient-oid>
```

Protocol v28 adds `QueueCureDisease`. The caller supplies only the fixture
patient OID and the client admits the authentic nonqueued `cureDisease`
command through `ClientCommandQueue`. The server remains authoritative for
disease state, organic-patient and PvP-help validation, antidote selection and
power, shared treatment recovery, Focus-adjusted Mind cost, charge use,
effects, and medical XP.

```powershell
.\scripts\Invoke-PrecuBackgroundInput.ps1 `
  -Action QueueCureDisease `
  -ClientProcessId <pid> `
  -TargetOid <fixture-patient-oid>
```

Protocol v29 adds `QueueRevivePlayer`. The caller supplies only the loaded,
dead fixture patient OID and the client admits the authentic nonqueued
`revivePlayer` command through `ClientCommandQueue`. The server remains
authoritative for the seven-meter player/death gate, resuscitation window,
group-or-consent and PvP-help rules, revive-pack selection, six-channel
damage/wound healing, Focus-adjusted Mind cost, charge use, medical XP,
upright recovery, and 60-second grogginess.

```powershell
.\scripts\Invoke-PrecuBackgroundInput.ps1 `
  -Action QueueRevivePlayer `
  -ClientProcessId <medic-pid> `
  -TargetOid <dead-fixture-patient-oid>
```

Protocol v30 adds `QueueDeathBlow`. The caller supplies only the loaded,
incapacitated hostile player OID. The client retains the authentic queued
three-second, 16-meter Publish 14.1 command row; the server remains
authoritative for the five-meter range, line-of-sight, PvP, incap, feign,
already-dead, and self-target gates.

The retained Publish 14.1 aliases are intentionally client-invisible while
remaining queued combat actions. The later client ordinarily gives such a row
sequence zero, which conflicts with its queue-clear sentinel. The compatibility
queue allocates a normal tracked sequence only for `deathBlow` and
`coupDeGrace`; the authentic rows and authoritative server path remain
unchanged.

```powershell
.\scripts\Invoke-PrecuBackgroundInput.ps1 `
  -Action QueueDeathBlow `
  -ClientProcessId <attacker-pid> `
  -TargetOid <incapacitated-hostile-oid>
```

Protocol v32 adds an identity-bound real-client performance seam for the
dedicated acceptance character. `StartDanceRhythmic`, `FlourishOne`, and
`StopDance` enqueue the authentic Publish 14.1 commands with fixed parameters
through the ordinary toolbar/client command queue. The client does not grant
skills, change HAM, attach performance scripts, or synthesize results; those
remain authoritative on the server.

```powershell
.\scripts\Invoke-PrecuBackgroundInput.ps1 `
  -Action StartDanceRhythmic `
  -ClientProcessId <dedicated-precu-client-pid>

.\scripts\Invoke-PrecuBackgroundInput.ps1 `
  -Action FlourishOne `
  -ClientProcessId <dedicated-precu-client-pid>

.\scripts\Invoke-PrecuBackgroundInput.ps1 `
  -Action StopDance `
  -ClientProcessId <dedicated-precu-client-pid>
```

Protocol v33 extends the same narrow command seam to the first solo music
session. `StartMusicStarwars1` queues `startMusic starwars1`, `FlourishOne`
queues the ordinary first flourish, and `StopMusic` queues the ordinary stop.
The server still owns the equipped instrument, both ability checks, Action
costs, heartbeat, and the Publish 14.1 delayed outro.

```powershell
.\scripts\Invoke-PrecuBackgroundInput.ps1 `
  -Action StartMusicStarwars1 `
  -ClientProcessId <dedicated-precu-client-pid>

.\scripts\Invoke-PrecuBackgroundInput.ps1 `
  -Action StopMusic `
  -ClientProcessId <dedicated-precu-client-pid>
```

Protocol v34 adds the corresponding group-owned band commands.
`StartBandStarwars1`, `BandFlourishOne`, and `StopBand` queue the production
`startBand starwars1`, `bandFlourish 1`, and `stopBand` commands. Use the
existing identity-bound group actions to invite and join the two acceptance
characters; the server remains authoritative for group membership, range,
instrument and skill admission, synchronized start time, per-member Action
cost, and the delayed band outro. The bridge reports fixed-command submission;
the authoritative fixture observation proves admission because immediate
Publish 14 commands may validly return client sequence zero.

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

## Core3 RANDOM area-action diagnostics

Background-input protocol 93 adds the first deferred area-action pilot:
`QueuePolearmSpinAttack1` and `PolearmSpinAttack1WeaponStatus`. The status
query reports whether the compiled command row exists, the current weapon type,
the valid and invalid weapon masks, and the final weapon-admission result.

```powershell
.\scripts\Invoke-PrecuBackgroundInput.ps1 `
  -Action PolearmSpinAttack1WeaponStatus `
  -ClientProcessId <dedicated-precu-client-pid>

.\scripts\Invoke-PrecuBackgroundInput.ps1 `
  -Action QueuePolearmSpinAttack1 `
  -Repeat 1 `
  -ClientProcessId <dedicated-precu-client-pid>
```

The helper only drives production client admission. The server remains
authoritative for area target selection and Core3's per-defender RANDOM HAM
pool resolution.

## Core3 one-handed/two-handed spin diagnostics

Background-input protocol 99 adds identity-bound Rantok and cleaver equip
actions plus production queue and weapon-mask diagnostics for
`melee1hSpinAttack1` and `melee2hSpinAttack1`:

```powershell
.\scripts\Invoke-PrecuBackgroundInput.ps1 -Action EquipFixtureOneHand -ClientProcessId <pid>
.\scripts\Invoke-PrecuBackgroundInput.ps1 -Action Melee1hSpinAttack1WeaponStatus -ClientProcessId <pid>
.\scripts\Invoke-PrecuBackgroundInput.ps1 -Action QueueMelee1hSpinAttack1 -Repeat 1 -ClientProcessId <pid>

.\scripts\Invoke-PrecuBackgroundInput.ps1 -Action EquipFixtureTwoHand -ClientProcessId <pid>
.\scripts\Invoke-PrecuBackgroundInput.ps1 -Action Melee2hSpinAttack1WeaponStatus -ClientProcessId <pid>
.\scripts\Invoke-PrecuBackgroundInput.ps1 -Action QueueMelee2hSpinAttack1 -Repeat 1 -ClientProcessId <pid>
```

## Core3 body-shot continuation diagnostics

Background-input protocol 103 adds production queue and generated-combat
status routes for `bodyShot2` and `bodyShot3`. Use the existing
`EquipCdefPistol` route after the identity-bound server fixture has prepared
its reversible pistol object:

```powershell
.\scripts\Invoke-PrecuBackgroundInput.ps1 -Action EquipCdefPistol -ClientProcessId <pid>
.\scripts\Invoke-PrecuBackgroundInput.ps1 -Action BodyShot2WeaponStatus -ClientProcessId <pid>
.\scripts\Invoke-PrecuBackgroundInput.ps1 -Action QueueBodyShot2 -Repeat 1 -ClientProcessId <pid>
.\scripts\Invoke-PrecuBackgroundInput.ps1 -Action BodyShot3WeaponStatus -ClientProcessId <pid>
.\scripts\Invoke-PrecuBackgroundInput.ps1 -Action QueueBodyShot3 -Repeat 1 -ClientProcessId <pid>
```

The helper also falls back to direct top-level HWND enumeration when the
non-activating Direct3D window recreation clears .NET's `MainWindowHandle`.
The bridge still targets only the selected process and never requests focus.

## Core3 head-shot continuation diagnostics

Background-input protocol 106 extends the production bridge for the exact
Core3 rifle continuation. `QueueDurationControl` remains the `headShot2`
queue route retained from the duration pilot; the explicit status route and
the new `headShot3` pair make both commands independently auditable:

```powershell
.\scripts\Invoke-PrecuBackgroundInput.ps1 -Action EquipCdefRifle -ClientProcessId <pid>
.\scripts\Invoke-PrecuBackgroundInput.ps1 -Action HeadShot2WeaponStatus -ClientProcessId <pid>
.\scripts\Invoke-PrecuBackgroundInput.ps1 -Action QueueDurationControl -Repeat 1 -ClientProcessId <pid>
.\scripts\Invoke-PrecuBackgroundInput.ps1 -Action HeadShot3WeaponStatus -ClientProcessId <pid>
.\scripts\Invoke-PrecuBackgroundInput.ps1 -Action QueueHeadShot3 -Repeat 1 -ClientProcessId <pid>
```

The status records expose the compiled rifle mask and replicated weapon type.
Damage, Mind-pool selection, spam, hit location, and generated animation remain
authoritative server results.

## Core3 one-hand body-hit-one diagnostics

Background-input protocol 108 adds production queue and generated-weapon
status routes for `melee1hBodyHit1`. Reuse the identity-bound Rantok fixture:

```powershell
.\scripts\Invoke-PrecuBackgroundInput.ps1 -Action EquipFixtureOneHand -ClientProcessId <pid>
.\scripts\Invoke-PrecuBackgroundInput.ps1 -Action Melee1hBodyHit1WeaponStatus -ClientProcessId <pid>
.\scripts\Invoke-PrecuBackgroundInput.ps1 -Action QueueMelee1hBodyHit1 -Repeat 1 -ClientProcessId <pid>
```

The status route exposes the compiled one-handed mask and replicated weapon
type. Health damage, `saimai` prose, hit location, and generated intensity
animation remain authoritative server results.

## Core3 one-hand body-hit continuation diagnostics

Background-input protocol 112 extends the same identity-bound Rantok path for
`melee1hBodyHit2` and `melee1hBodyHit3`:

```powershell
.\scripts\Invoke-PrecuBackgroundInput.ps1 -Action EquipFixtureOneHand -ClientProcessId <pid>
.\scripts\Invoke-PrecuBackgroundInput.ps1 -Action Melee1hBodyHit2WeaponStatus -ClientProcessId <pid>
.\scripts\Invoke-PrecuBackgroundInput.ps1 -Action QueueMelee1hBodyHit2 -Repeat 1 -ClientProcessId <pid>
.\scripts\Invoke-PrecuBackgroundInput.ps1 -Action Melee1hBodyHit3WeaponStatus -ClientProcessId <pid>
.\scripts\Invoke-PrecuBackgroundInput.ps1 -Action QueueMelee1hBodyHit3 -Repeat 1 -ClientProcessId <pid>
```

The two status routes expose compiled mask and replicated weapon type. Health
damage, `saisun`/`saitok` prose, hit location, and generated intensity
animation remain authoritative server results.

## Core3 two-hand head-hit diagnostics

Background-input protocol 118 adds queue and cleaver-status routes for
`melee2hHeadHit1`, `melee2hHeadHit2`, and `melee2hHeadHit3`. Equip the
identity-bound two-hand fixture, then use the matching `QueueMelee2hHeadHit*`
and `Melee2hHeadHit*WeaponStatus` actions. Mind damage, scalp prose, hit
location, and generated intensity animation remain server-authoritative.

The Release Win32 `DataTableTool` project also carries the minimal modern
linker compatibility needed to compile these legacy tables with the v143
toolchain: the existing Unicode library, legacy stdio definitions, and
non-SAFESEH third-party objects. Its legacy lexer requires relative input and
output filenames; invoke it from the table's directory rather than passing an
absolute Windows path.

## Core3 basic melee-hit diagnostics

Background-input protocol 126 adds independent queue and weapon-status routes
for `melee1hHit1`, `melee1hHit2`, `melee2hHit1`, and `melee2hHit2`. Use
`EquipFixtureOneHand` with the `Melee1hHit*WeaponStatus`/`QueueMelee1hHit*`
actions, or `EquipFixtureTwoHand` with their `Melee2hHit*` counterparts.
Random HAM resolution, exact combat spam, hit location, and generated
intensity animation remain authoritative server results.

## Core3 polearm leg-hit continuation diagnostics

Background-input protocol 130 adds independent queue and wooden-staff status
routes for `polearmLegHit2` and `polearmLegHit3`. Equip the identity-bound
polearm fixture, then use `PolearmLegHit2WeaponStatus`/`QueuePolearmLegHit2`
or `PolearmLegHit3WeaponStatus`/`QueuePolearmLegHit3`. Action damage,
`legsmasher`/`legbreaker` prose, hit location, and distinct generated
intensity animations remain authoritative server results.

## Core3 polearm hit-and-area diagnostics

Background-input protocol 134 adds independent queue and wooden-staff status
routes for `polearmHit1` and `polearmArea1`. Use
`PolearmHit1WeaponStatus`/`QueuePolearmHit1` or
`PolearmArea1WeaponStatus`/`QueuePolearmArea1` after equipping the
identity-bound polearm fixture. RANDOM HAM resolution, exact combat prose,
the 16-meter area shape, and generated intensity animations remain
authoritative server results.

## Core3 two-hand spin-attack continuation diagnostics

Background-input protocol 135 adds independent queue and cleaver-status routes
for `melee2hSpinAttack2`. Equip the identity-bound two-handed fixture, then
use `Melee2hSpinAttack2WeaponStatus` and `QueueMelee2hSpinAttack2`.
RANDOM HAM resolution, `spinslam` prose, the 16-meter area shape, and
generated `combo_4b` intensity playback remain authoritative server results.

## Core3 Body Shot I closure diagnostics

M216 reuses protocol 135's existing `QueueBodyShot1`, `EquipCdefPistol`, and
`CombatQueueStatus` routes; no client binary or protocol change is required.
The production queue proved the corrected one-second command row and
`bodyshot_hit` server output. The reusable Marksman fixture now distinguishes
real reversible state from diagnostic-only objvars, allowing a fresh prepare
without disabling retained combat diagnostics.

## Core3 Burst Shot I diagnostics

Background-input protocol 136 adds `QueueBurstShot1` at fixed ID 170 and
`BurstShot1WeaponStatus` at fixed ID 171. Equip the identity-bound CDEF
carbine with `EquipCdefCarbine` and wait for replication before queueing.
The status route exposes the compiled carbine mask; RANDOM-pool resolution,
`burstshot` prose, and generated `fire_7_single` ranged playback remain
authoritative server results.

## Core3 Disarming Shot I diagnostics

Background-input protocol 137 adds `QueueDisarmingShot1` at fixed ID 172 and
`DisarmingShot1WeaponStatus` at fixed ID 173. Equip the identity-bound CDEF
pistol with `EquipCdefPistol` and wait for replication before queueing. The
status route exposes pistol type 2 and mask `0x0004`; RANDOM-pool resolution,
`disarmshot` prose, and generated `fire_3_single` ranged playback remain
authoritative server results.

## Core3 Double Tap diagnostics

Background-input protocol 138 adds `QueueDoubleTap` at fixed ID 174 and
`DoubleTapWeaponStatus` at fixed ID 175. Equip the identity-bound CDEF pistol
with `EquipCdefPistol` and wait for replication before queueing. The status
route exposes pistol type 2 and mask `0x0004`; RANDOM-pool resolution,
`doubletap` prose, and generated `fire_7_single` ranged playback remain
authoritative server results.

## Core3 Stopping Shot diagnostics

Background-input protocol 139 adds `QueueStoppingShot` at fixed ID 176 and
`StoppingShotWeaponStatus` at fixed ID 177. Equip the identity-bound CDEF
pistol with `EquipCdefPistol` and wait for replication before queueing. The
status route exposes pistol type 2 and mask `0x0004`; RANDOM-pool resolution,
`stoppingshot` prose, and generated `fire_1_special_single` ranged playback
remain authoritative server results.

## Core3 Crippling Shot diagnostics

Background-input protocol 140 adds `QueueCripplingShot` at fixed ID 178 and
`CripplingShotWeaponStatus` at fixed ID 179. Equip the identity-bound CDEF
carbine with `EquipCdefCarbine` and wait for replication before queueing. The
status route exposes carbine type 1 and mask `0x0002`; RANDOM-pool resolution,
`cripplingshot` prose, and generated `fire_5_single` ranged playback remain
authoritative server results.

## Core3 Point Blank Single II diagnostics

Background-input protocol 141 adds `QueuePointBlankSingle2` at fixed ID 180
and `PointBlankSingle2WeaponStatus` at fixed ID 181. Equip the identity-bound
CDEF pistol with `EquipCdefPistol`, place the counterpart within 10 meters,
and wait for replication before queueing. The status route exposes pistol
type 2 and mask `0x0004`; RANDOM-pool resolution, `pointblankblast` prose,
and generated `fire_5_single` ranged playback remain authoritative server
results.

## Core3 Point Blank Area I diagnostics

Background-input protocol 142 adds `QueuePointBlankArea1` at fixed ID 182 and
`PointBlankArea1WeaponStatus` at fixed ID 183. The command accepts any ranged
weapon; the identity-bound CDEF pistol therefore validates type 2 against the
generated `RANGED` mask before the client queues Core3's 15-meter area action
at its explicit 12-meter command range. Protocol 142 dedicates the middle 32
status bits to the complete valid-weapon mask, preserving aggregate-family
bits; live acceptance reports `0x08000000` rather than a truncated zero.

## Core3 Point Blank Area II diagnostics

Background-input protocol 143 adds `QueuePointBlankArea2` at fixed ID 184 and
`PointBlankArea2WeaponStatus` at fixed ID 185. The identity-bound CDEF pistol
proves concrete pistol admission before the client queues Core3's 12-meter,
60-degree cone action. The server remains authoritative for cone selection,
RANDOM-pool resolution, `areashot` prose, and generated-intensity playback.

## Core3 Multi-Target Pistol Shot diagnostics

Background-input protocol 144 adds `QueueMultiTargetPistolShot` at fixed ID
186 and `MultiTargetPistolShotWeaponStatus` at fixed ID 187. The reversible
Master Pistoleer fixture is layered over the established Marksman/CDEF-pistol
fixture. The client proves concrete pistol admission and queues the production
command; the server remains authoritative for the 32-meter area, RANDOM HAM
selection, `pistolmultishot` prose, and generated-ranged animation.

## Core3 Disarming Shot II diagnostics

Background-input protocol 145 adds `QueueDisarmingShot2` at fixed ID 188 and
`DisarmingShot2WeaponStatus` at fixed ID 189. The reversible Master Pistoleer
fixture is layered over the established Marksman/CDEF-pistol fixture. The client
proves concrete pistol admission and queues the production command; the server
remains authoritative for the 15-degree cone, RANDOM HAM selection,
`disarmblast` prose, and generated-ranged animation.

## Core3 Fan Shot diagnostics

Background-input protocol 146 adds `QueueFanShot` at fixed ID 190 and
`FanShotWeaponStatus` at fixed ID 191. The existing reversible Master
Pistoleer fixture supplies Ability IV ownership. The client proves concrete
pistol admission and queues the production command; the server remains
authoritative for the 60-degree cone, RANDOM HAM selection, `fanshot` prose,
and generated-intensity animation.

## Core3 Burst Shot II diagnostics

Background-input protocol 147 adds `QueueBurstShot2` at fixed ID 192 and
`BurstShot2WeaponStatus` at fixed ID 193. The reversible Marksman/Carbineer
fixture owns Ability I through III and the CDEF carbine certification. The
client proves concrete carbine admission and queues the production command;
the server remains authoritative for the 30-degree cone, RANDOM HAM selection,
`burstblast` prose, and generated-ranged animation.

When Windows starts the opted-in client with `SW_HIDE`, the legacy foundation
window may remain unavailable to the bridge. Protocol 147 therefore creates a
zero-size hidden process-local fallback HWND, subclasses it with the same
bounded bridge procedure, and destroys it during normal teardown. It is never
shown or activated and does not broaden command admission.

## Core3 Unarmed Hit I diagnostics

Background-input protocol 148 adds `QueueUnarmedHit1` at fixed ID 194 and
`UnarmedHit1WeaponStatus` at fixed ID 195. The reversible headshot fixture
owns Brawler Unarmed I while the attacker remains genuinely unarmed. The
client proves weapon type 6 against mask `0x00000040` and queues the production
command; the server remains authoritative for RANDOM HAM selection,
`steelhands` prose, and generated-intensity playback.

## Core3 Unarmed Hit II diagnostics

Background-input protocol 149 adds `QueueUnarmedHit2` at fixed ID 196 and
`UnarmedHit2WeaponStatus` at fixed ID 197. The reversible headshot fixture
owns the Teras Kasi novice command while the attacker remains genuinely
unarmed. The client proves weapon type 6 against mask `0x00000040` and queues
the production command; the server remains authoritative for RANDOM HAM
selection, `goraxsmash` prose, and generated-intensity playback.

## Core3 batched unarmed diagnostics

Background-input protocol 150 adds `QueueUnarmedBodyHit1` and
`UnarmedBodyHit1WeaponStatus` at fixed IDs 198/199,
`QueueUnarmedLegHit1` and `UnarmedLegHit1WeaponStatus` at IDs 200/201, and
`QueueUnarmedSpinAttack1` and `UnarmedSpinAttack1WeaponStatus` at IDs
202/203. The reversible fixture owns each command while the attacker remains
genuinely unarmed. Every status route proves weapon type 6 against mask
`0x00000040`; the server remains authoritative for fixed Health, fixed
Action, or RANDOM area targeting, combat prose, and generated-intensity
playback.

## Core3 second-spin and overcharge diagnostics

Background-input protocol 151 adds `QueueUnarmedSpinAttack2` and
`UnarmedSpinAttack2WeaponStatus` at fixed IDs 204/205, plus
`QueueOverChargeShot2` and `OverChargeShot2WeaponStatus` at fixed IDs
206/207. The unarmed route proves type 6 against mask `0x00000040`; the
overcharge route proves a concrete CDEF rifle against aggregate-ranged mask
`0x08000000`. Both routes queue the production commands while the server
remains authoritative for RANDOM pool selection, combat prose, area or
single-target geometry, and generated animation playback.

## Core3 special-heavy diagnostics

Background-input protocol 152 adds `EquipFixtureAcid`,
`QueueFireAcidSingle1`, and `FireAcidSingle1WeaponStatus` at fixed IDs
208/209/210, plus `EquipFixtureLightning`, `QueueFireLightningSingle1`, and
`FireLightningSingle1WeaponStatus` at IDs 211/212/213. The acid status route
proves canonical ground-targeting weapon type 12 against HEAVY mask
`0x00000008`; the lightning route proves rifle type 0 against mask
`0x00000001`. Both authenticated routes returned production Success while
the server remained authoritative for exact template gating, RANDOM pool
selection, combat prose, HAM costs, and generated-intensity playback.

## Core3 Action Shot I posture-down diagnostics

Background-input protocol 176 adds `QueueActionShot1` at fixed ID 256 and
`ActionShot1WeaponStatus` at fixed ID 257. The identity-bound fixture creates
and equips a reversible CDEF carbine so the status route proves weapon type 1
against mask `0x00000002` before the authenticated client queues the production
command. The server remains authoritative for exact adjusted three-pool costs,
Action-only direct damage and bleeding, generated ranged animation, `sapshot`
combat prose, posture immunity and defense, posture transition, and the
30-second recovery rule.

## Core3 Action Shot II cone diagnostics

Background-input protocol 177 adds `QueueActionShot2` at fixed ID 258 and
`ActionShot2WeaponStatus` at fixed ID 259. The existing reversible CDEF
carbine proves weapon type 1 against mask `0x00000002` before the client queues
the production command. The server remains authoritative for the 15-degree
cone, 2.0 damage and speed multipliers, exact adjusted three-pool costs,
Action-only direct damage and bleeding, `fire_5_special_single` generated
animation, `sapblast` prose, posture defense, transition, and recovery.

## Core3 Marksman-novice shot closure

Background-input protocol 178 adds `QueueOverChargeShot1` and
`OverChargeShot1WeaponStatus` at fixed IDs 260/261, plus
`QueuePointBlankSingle1` and `PointBlankSingle1WeaponStatus` at fixed IDs
262/263. A reversible CDEF rifle proves weapon type 0 against aggregate-ranged
mask `0x08000000` before the authenticated client queues either production
command. The server remains authoritative for exact adjusted three-pool costs,
RANDOM pool resolution, single-target geometry, the point-blank 12-meter
boundary, direct damage, `overchargeshot`/`pointblankshot` prose, and generated
ranged animation.

## Core3 Marksman support shots

Background-input protocol 179 adds `QueueThreatenShot` and
`ThreatenShotWeaponStatus` at fixed IDs 264/265, plus `QueueWarningShot` and
`WarningShotWeaponStatus` at fixed IDs 266/267. A reversible CDEF rifle proves
weapon type 0 against aggregate-ranged mask `0x08000000` before the
authenticated client queues either production command. The server remains
authoritative for exact adjusted three-pool costs, RANDOM pool resolution,
single-target geometry, direct damage, `threatenshot`/`warningshot` prose,
and the retained ranged-versus-intensity animation modes.

## Core3 Aim lifecycle

Background-input protocol 180 adds `QueueAim` and `AimWeaponStatus` at fixed
IDs 268/269. A reversible CDEF rifle proves weapon type 0 against
aggregate-ranged mask `0x08000000` before the authenticated client queues the
production `aim` command. The server owns the exact 12/0/0 adjusted cost and
the pinned Core3 lifecycle: a five-second `STATE_AIMING` state carrying the
sum of `aim` and weapon-family aim modifiers as `private_aim`, repeat charging
without duration refresh, natural expiry, and removal after the next
successful combat action.

## Core3 Suppression Fire I

Background-input protocol 181 adds `QueueSuppressionFire1` and
`SuppressionFire1WeaponStatus` at fixed IDs 270/271. A reversible CDEF rifle
proves weapon type 0 against aggregate-ranged mask `0x08000000` before the
authenticated client queues production `suppressionFire1`. The server owns
the exact adjusted three-pool costs, Health-targeted direct damage,
`suppressionfire` prose, exact `fire_defender_posture_change_down` playback,
the 100% posture-down attempt, defender recovery defense, and the 30-second
recovery lifecycle.

## Core3 Pistol Acrobatics

Background-input protocol 182 adds `QueueRollShot`, `RollShotWeaponStatus`,
`QueueDiveShot`, `DiveShotWeaponStatus`, `QueueKipUpShot`, and
`KipUpShotWeaponStatus` at fixed IDs 272-277. The identity-bound CDEF pistol
path proves the retained Pistol II grants and the natural
upright-to-crouched-to-prone-to-upright Core3 posture lifecycle, including
server-authoritative damage, HAM debit, diagnostics, and reversible cleanup.

## Core3 Take Cover

Background-input protocol 183 adds `QueueTakeCover` and
`TakeCoverWeaponStatus` at fixed IDs 278/279. The identity-bound fixture
proves retained Marksman Rifle II ownership and resets only its snapshotted
cover state before the authenticated client queues production `takeCover`.
The server owns the exact Quickness-adjusted Action cost, insufficient-HAM
failure, combat-only `10 + take_cover` chance, dizzy-fall branch, cover-state
application, persistence observation, and reversible cleanup.

## Core3 Full Auto Single I

Background-input protocol 184 adds `QueueFullAutoSingle1` and
`FullAutoSingle1WeaponStatus` at fixed IDs 280/281. The identity-bound CDEF
carbine path proves retained Marksman Carbine II ownership before the
authenticated client queues production `fullAutoSingle1`. The server owns
the exact adjusted three-pool costs, RANDOM target-pool damage,
`fullautoattack` prose, `fire_5_special_single` animation, and the pinned
Core3 dizzy, blind, and stun attempts with their independent defenses,
duration scaling, native timed states, persistence observation, and
ownership-safe cleanup.

## Core3 Scatter Shots

Background-input protocol 185 adds `QueueScatterShot1`,
`ScatterShot1WeaponStatus`, `QueueScatterShot2`, and
`ScatterShot2WeaponStatus` at fixed IDs 282-285. The identity-bound CDEF
carbine path proves retained Carbineer Accuracy I and III ownership before an
authenticated client queues production `scatterShot1` and `scatterShot2`.
The server owns the pinned Core3 independent pool rolls, direct
Health/Action/Mind multipliers, 8.34-percent historical spillover rule, exact
HAM costs, `scattershot`/`scatterblast` prose, generated
`fire_5_single` animation, persistence observation, and reversible cleanup.

## Core3 Leg Shot Continuation

Background-input protocol 186 adds `QueueLegShot2`,
`LegShot2WeaponStatus`, `QueueLegShot3`, and `LegShot3WeaponStatus` at fixed
IDs 286-289. The identity-bound CDEF-carbine path proves retained Marksman
Carbine III and Carbineer Speed I ownership before authenticated clients
queue production `legShot2` and `legShot3`. The server owns exact Action-pool
damage and HAM costs, `legshot`/`kneecapshot` prose, exact `test_homing`
animation, pinned Core3 85/100-percent defended timed stun attempts,
persistence observation, and reversible cleanup.

## Core3 Wild Shot I

Background-input protocol 189 adds `QueueWildShot1` and
`WildShot1WeaponStatus` at fixed IDs 294-295. The identity-bound CDEF-carbine
path proves retained Carbineer Accuracy II ownership before an authenticated
client queues production `wildShot1`. The server owns RANDOM-pool damage,
exact HAM costs, `wildshot` prose, generated `fire_7_single` animation, the
defended 50-percent native stun attempt, persistence observation, and
reversible cleanup.

## Core3 Wild Shot II

Background-input protocol 190 adds `QueueWildShot2` and
`WildShot2WeaponStatus` at fixed IDs 296-297. The identity-bound CDEF-carbine
path proves retained Carbineer Accuracy IV ownership before an authenticated
client queues production `wildShot2`. The server owns the 30-degree cone,
RANDOM-pool damage, exact HAM costs, `widewildshot` prose, generated
`fire_7_single` animation, the defended 50-percent native stun attempt,
persistence observation, and reversible cleanup.

## Core3 Suppression Fire II

Background-input protocol 188 adds `QueueSuppressionFire2` and
`SuppressionFire2WeaponStatus` at fixed IDs 292-293. The identity-bound CDEF
carbine path proves retained Carbineer Ability IV ownership before an
authenticated client queues production `suppressionFire2`. The server owns
RANDOM-pool damage, exact HAM costs, `sup_fire` prose, exact
`fire_defender_posture_change_down` playback, the defended native
posture-down/recovery transaction, persistence observation, and reversible
cleanup.

## Core3 Full Auto Single II

Background-input protocol 187 adds `QueueFullAutoSingle2` and
`FullAutoSingle2WeaponStatus` at fixed IDs 290-291. The identity-bound
CDEF-carbine path proves retained Carbineer-novice ownership before an
authenticated client queues production `fullAutoSingle2`. The server owns
RANDOM-pool damage, exact HAM costs, `s_auto` prose, generated
`fire_7_single` animation, all three defended timed-state attempts,
persistence observation, and reversible cleanup.
