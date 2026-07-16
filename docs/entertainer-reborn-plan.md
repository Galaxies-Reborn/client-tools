# Entertainer Reborn Integration Plan

Status: planning only. No Entertainer Reborn runtime behavior is implemented by
this document.

Baseline: `x64-dx9-vanilla` at commit
`9fdf07779f9cfe93625ec19d42a0d4af82f81266`.

## Goals

Entertainer Reborn adds three performance modes to the x64 D3D9 client:

1. **Start Midi to Flourish** - select an existing SWG song and map MIDI or
   performance-key presses to its eight existing flourish phrases.
2. **Start Midi to Music** - play an entertainer instrument note by note from a
   MIDI controller or the computer keyboard.
3. **Start Music from Script** - load a Standard MIDI File from the game's
   `midi` directory and perform its sequence through an entertainer instrument.

The feature must preserve existing entertainer skill checks, instrument
requirements, posture, animation, audience range, and stop conditions. Normal
gameplay input bindings must remain unchanged.

## Non-goals for the first release

- Replacing the existing `/startMusic`, `/flourish`, or `/stopMusic` behavior.
- Treating existing phrase loops as clean note samples. Those recordings are
  mixed phrases and generally cannot become convincing instruments by pitching
  the whole file.
- Executing arbitrary scripts or reading files outside the game `midi` folder.
- Shipping third-party MIDI songs or instrument samples without confirmed
  redistribution rights.
- Adding gamepad performance mapping in the first implementation. The UI can
  leave room for it without exposing an unfinished tab.

## Existing systems to reuse

- `PlayerMusicManager` already loads `datatables/performance/performance.iff`
  and manages intro, main loop, eight flourishes, outro, audio, and animation.
- `CM_musicFlourish` already carries server-authorized flourish events to
  clients, and `PlayerMusicManager::queueFlourish()` schedules the phrase.
- Server performance scripts already validate and manage music through
  `library/performance.java`, `player/skill/performcommands.java`, and
  `systems/skills/performance/active_music.java`.
- The x64 JUCE backend already owns the Windows audio device and decodes game
  audio. JUCE 8.0.14 also supplies MIDI device enumeration, MIDI parsing,
  scheduling, and sampler/synthesiser primitives that are not enabled yet.
- `SwgCuiOpt` and the existing mediator framework provide the native UI path.
  The normal `InputMap` remains the owner of gameplay bindings.

## Product flow

### Start Midi to Flourish

Display label: `Start Midi to Flourish`

Internal command: `/startMidiToFlourish [song]`

1. With no argument, open a song picker containing only songs the character may
   perform with the equipped instrument.
2. Start the selected song through the existing server command and performance
   script path.
3. Enter Flourish mode after the server confirms the performance.
4. Map MIDI note-on or performance-key presses to flourish slots 1 through 8.
   MIDI note-off is ignored in this mode.
5. Send the existing flourish command, preserving server validation, phrase
   queuing, animation, particles, and what nearby clients hear.
6. Apply client debounce and server rate limits so a controller cannot flood the
   command queue.

This is the lowest-risk first vertical slice because it uses the original audio
and network behavior end to end.

### Start Midi to Music

Display label: `Start Midi to Music`

Internal command: `/startMidiToMusic [instrumentPatch]`

1. Validate entertainer eligibility and the equipped instrument on the server.
2. Open or reuse the configured MIDI input. Keyboard input is always available.
3. Start a versioned note-performance session and show the performance HUD.
4. Monitor the local performer's notes immediately through the JUCE sampler.
5. Batch timestamped note events to the server. The server validates and relays
   them to interested nearby clients.
6. Remote clients render notes through the same instrument patch using a small
   jitter buffer.

Supported first-release events should be note on, note off, velocity, sustain,
all-notes-off, octave shift, and semitone transpose. Pitch bend and additional
continuous controllers should be deferred until note playback is stable.

### Start Music from Script

Display label: `Start Music from Script`

Internal command: `/startMusicFromScript [filename]`

1. Create `<game root>\midi` on first use if it does not exist.
2. With no argument, open a picker rooted to that directory.
3. Accept `.mid` and `.midi` Standard MIDI Files only. Resolve and canonicalize
   the path, then reject traversal, links, devices, and files outside the root.
4. Parse the file as data with JUCE `MidiFile`; never execute file contents.
5. Show filename, format, tracks, duration, tempo, and selected instrument patch
   before starting.
6. Use the same server-authorized session and note event path as live music.
   The local scheduler derives timestamped note batches from MIDI ticks and the
   tempo map.
7. Provide play, pause, resume, stop, seek-to-start, track mute, and tempo scale.
   Multiplayer playback should only begin after the server supplies a common
   start time.

Malformed files, unsupported events, excessive polyphony, excessive duration,
and oversized files must fail with a useful UI error. Safe initial limits are
1 MiB, 32 tracks, 64 simultaneous notes, and two hours; make these data-driven.

### Stop lifecycle

Display label: `Stop Performance Mode`

Internal command: `/stopPerformanceMode`

All three modes need one reliable exit path. It sends all-notes-off, stops the
session on the server, restores normal input routing, closes the compact HUD,
and returns the character through the existing music stop lifecycle. It must
also run on disconnect, scene change, incapacitation, instrument loss, audio
device loss, and client shutdown.

## Client architecture

Add a `PerformanceModeManager` in `clientGame` as the single owner of mode and
session state. It coordinates three focused components:

- `PerformancePhraseController` maps eight triggers to existing flourish
  commands without duplicating `PlayerMusicManager` scheduling.
- `PerformanceNoteEngine` converts MIDI or keyboard events into canonical note
  events and submits them to JUCE and the network session.
- `PerformanceSequencePlayer` parses Standard MIDI Files and feeds the same note
  engine from a deterministic transport clock.

The canonical event should contain session id, sequence number, server-relative
timestamp, event type, channel, note, velocity, and controller value. Live,
keyboard, and scripted sources must converge on this representation before
audio or networking. This keeps local and remote playback behavior aligned.

Extend the JUCE backend behind a narrow client-audio API instead of exposing
JUCE types throughout game code:

- enumerate and identify MIDI inputs by stable device identifier;
- open, close, and reconnect the selected device;
- deliver timestamped MIDI messages on a thread-safe queue;
- own sampler voices and instrument patches on the audio thread;
- expose all-notes-off and device-loss notifications;
- report audio/MIDI diagnostics without writing noisy release logs.

MIDI callbacks must never touch game objects, UI objects, or network objects
directly. They enqueue compact events for the game thread. Sampler state changes
must use JUCE's audio-thread-safe mechanisms.

## Performance-only input

Create a separate `PerformanceInputMap`; do not add note bindings to the normal
gameplay `InputMap` or write them into existing keymap files.

When a performance mode is active, a small input context receives configured
key-down and key-up events before gameplay commands. It consumes only keys bound
to performance actions. When no performance mode is active, it is inert and the
same keys behave exactly as they did before.

Required bindable actions:

- chromatic notes covering at least two octaves;
- octave up and octave down;
- transpose up and transpose down by one semitone;
- sustain;
- all notes off;
- flourish 1 through 8;
- stop performance mode.

Do not trigger performance notes while chat, console, naming, search, or another
text editor owns keyboard focus. Escape exits assignment capture first, then the
performance screen, then the active performance mode. Losing application focus
must release every held keyboard note.

Persist mappings separately per Windows user/profile, with defaults that mirror
a two-row piano layout. Store stable MIDI device identifiers, channel filters,
velocity curve, octave, transpose, and mappings in a versioned
`entertainer_reborn.json` profile file. Load unknown future fields leniently and
write atomically through a temporary file plus rename.

## User interface

Add a native `Performance` options page and a compact performance HUD using the
existing Cui mediator and client asset systems. The reference image guides the
interaction model, not the visual assets.

The options page contains:

- **Keyboard** tab with a two-octave piano, computer-key labels, clickable note
  selection, assignment capture, clear, reset, apply, and conflict feedback;
- **MIDI** tab with device selector, refresh button, connection status, channel
  filter, velocity curve, live activity indicator, and a MIDI-learn action for
  flourish mappings;
- mode-specific mappings for eight flourishes and global transport actions;
- audible preview that never starts a network performance.

The in-performance HUD contains:

- a stable-width piano visualization with held local and remote notes;
- current mode, song or script, instrument patch, octave, and transpose;
- script transport controls when applicable;
- MIDI connection indicator and an icon button that opens settings;
- an explicit stop button.

Use the established SWG visual language and UI assets. The layout must support
windowed play, common aspect ratios, UI scaling, and long localized labels
without overlap.

## Instrument patches and assets

Existing entertainer audio is phrase-oriented. Note mode needs purpose-built
multisample patches, generated synthesis patches, or both. Define a versioned
instrument patch table with:

- patch id and compatible in-game instrument ids;
- sample asset path, root note, key and velocity ranges;
- fine tuning, gain, pan, loop points, and one-shot/loop behavior;
- attack, decay, sustain, release, polyphony, and voice-stealing policy;
- optional filter and reverb sends.

Prefer multiple isolated samples across each instrument's range. Pitching a
single clean note can bootstrap a prototype, but pitching an existing full song
loop is not an acceptable production patch. Every sample needs a documented
source and redistribution license.

Patch definitions and packaged samples belong in `client-assets`, compiled into
the normal asset delivery format. Loose MIDI files remain under the end user's
`<game root>\midi` directory and are not placed in TRE archives.

## Multiplayer protocol and server authority

Add versioned shared network messages for:

- capability negotiation;
- session start acknowledgement and stop;
- timestamped note/controller batches;
- script transport start, pause, resume, and stop;
- session state for observers entering range or loading late.

The server owns session identity, start time, performer eligibility, equipped
instrument, mode transitions, audience interest/range, and rate limits. It
clamps MIDI values, rejects stale or out-of-order batches, limits events and
polyphony, and ends sessions through the same conditions as normal music.

The performer hears immediate local monitoring. Nearby clients buffer relayed
events briefly against the server clock to reduce jitter. Sequence numbers and
periodic state snapshots allow recovery from packet loss without stuck notes.

Do not transmit local paths or arbitrary raw file contents. Script mode sends
validated canonical events and transport metadata. A later optimization may use
content hashes for server-approved shared songs.

Clients without the negotiated capability must continue to connect safely. For
Flourish mode they still receive the original music messages. For note sessions
they should ignore unknown optional messages or receive a configured fallback
phrase, depending on what protocol compatibility testing supports.

## Repository ownership

Keep changes in their owning repositories:

- `Galaxies-Reborn/x64-dx9-vanilla-entertainerreborn` (this private repository):
  client source, JUCE MIDI/sampler bridge, performance state, input context,
  client networking, build files, tests, and technical documentation.
- `Galaxies-Reborn/swg-main`: server commands and scripts, shared/server network
  handling, validation, interest relay, rate limiting, and server tests.
- `Galaxies-Reborn/client-assets`: UI pages/styles/strings, command and data
  tables, instrument patch definitions, and redistributable sample assets.

Create matching `x64-dx9-vanilla-entertainerreborn` branches in the server and
asset repositories when implementation begins. Do not copy those repositories
inside this one or add machine-local path dependencies. Pin cross-repository
integration by commit in release notes and the launcher manifest.

## Delivery phases

### Phase 0 - contracts and test scaffolding

- Freeze command names, session/event schemas, patch schema, config schema, and
  repository branches.
- Add capability negotiation and no-op message compatibility tests.
- Add a local diagnostic panel for MIDI enumeration and note activity.

Exit criteria: clean x64 gameplay and God client builds; vanilla behavior is
unchanged; a virtual and physical MIDI device can be selected and reconnected.

### Phase 1 - Midi to Flourish vertical slice

- Implement the performance input context and basic Keyboard/MIDI settings.
- Add song selection and the `Start Midi to Flourish` command.
- Route eight inputs through the existing server flourish command path.
- Add the compact Flourish HUD and unified stop lifecycle.

Exit criteria: two clients observe identical original flourishes; normal keymaps
remain byte-for-byte unchanged; chat input and focus loss cannot create notes.

### Phase 2 - local note engine and first patch

- Implement the canonical note engine and JUCE sampler bridge.
- Add one legally redistributable, multisampled entertainer instrument patch.
- Complete piano mapping, MIDI learn, velocity, sustain, transpose, and octave.

Exit criteria: no stuck notes through device loss or mode changes; stable audio
under the polyphony limit; local keyboard and MIDI produce equivalent notes.

### Phase 3 - networked live music

- Implement server-authorized note sessions and observer relaying.
- Add batching, clock synchronization, jitter buffering, recovery snapshots,
  validation, and rate limits.
- Tie eligibility, animation, posture, instrument, and stop conditions to the
  existing performance system.

Exit criteria: multiple nearby clients hear synchronized live notes, late join
recovers correctly, out-of-range clients receive nothing, and hostile input is
bounded by server limits.

### Phase 4 - Music from Script

- Add sandboxed file browsing and Standard MIDI File validation.
- Implement deterministic tempo-map scheduling and transport controls.
- Add track selection/muting and per-track channel-to-patch mapping where the
  first patch set supports it.

Exit criteria: format 0 and format 1 MIDI files play consistently across clients;
malformed and oversized files fail safely; pause/resume/stop never leaves notes
active.

### Phase 5 - content, polish, and release

- Expand licensed instrument patches and tune animation/flourish presentation.
- Complete localization, accessibility, scaling, diagnostics, and clean staging.
- Update the launcher/install manifest to create the `midi` folder and select
  matching client, asset, and server revisions.

Exit criteria: clean install and upgrade tests, no debug artifacts, documented
licenses, multiplayer soak test, and signed release manifests.

## Verification matrix

- Build `Release|x64` gameplay and God clients from a clean checkout.
- Run unit tests for mapping persistence, path confinement, MIDI parsing, tempo
  conversion, transposition, note state, batching, and rate limits.
- Test with no MIDI device, a hot-plugged device, a virtual MIDI port, and at
  least one physical controller.
- Verify normal gameplay bindings before, during, and after every mode.
- Exercise chat, console, alt-tab, disconnect, scene load, death/incapacitation,
  instrument removal, and audio/MIDI device loss.
- Test two to five clients with latency, jitter, dropped/reordered events, late
  interest entry, and mixed capable/vanilla client versions.
- Inspect release staging for logs, dumps, absolute paths, test MIDI files, and
  unlicensed sample content.

## Decisions required before implementation

1. Choose the first in-game instrument and the licensed sample source for the
   Phase 2 patch.
2. Decide whether note sessions should grant normal entertainer XP/buffs. The
   conservative default is no rewards until server-side anti-automation rules
   and participation semantics are agreed.
3. Decide whether scripted playback is available to all musicians or gated by a
   server permission/skill. The conservative default is the same eligibility as
   live note mode plus configurable server policy.
4. Confirm mixed-client behavior for note sessions: silent graceful fallback or
   a low-bandwidth existing phrase fallback.
5. Confirm whether MIDI script tempo may be changed during multiplayer playback;
   the conservative first release locks tempo after the shared start.

## Recommended first implementation slice

Begin with Phase 0 and Phase 1 only. It proves command registration, UI asset
delivery, isolated input ownership, MIDI enumeration, server validation, and the
stop lifecycle while reusing the proven flourish audio path. Once that slice is
stable, the sampler and new multiplayer note protocol can be added without
mixing foundational input/UI defects with synthesis and timing defects.
