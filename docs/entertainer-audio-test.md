# Entertainer Audio Test

## Original-timbre sample bank

The JUCE note synthesizer can derive a local sample bank from the original
player-music recordings owned by the client installation. The generated WAVs
are not committed to Git. The builder requires Python 3 and NumPy; when NumPy
is absent, install the pinned requirement with
`python -m pip install -r scripts\requirements-entertainer-sample-bank.txt`.

```powershell
.\scripts\Build-EntertainerSampleBank.ps1
.\scripts\Run-EntertainerAudioTest.ps1
```

At startup the harness reports how many of the 14 instrument slots were loaded.
The game and harness look for `midi\instruments\instrument_01.wav` through
`instrument_14.wav` beside the executable. `Stage-X64Client.ps1` copies a
generated bank into that location when one is available. Missing samples use
the procedural JUCE patches automatically.

The original performance table aliases visible instruments into six recorded
stem families. Consequently, the generated bank preserves the shipped stem
timbres and canonical instrument grouping; it cannot recover isolated notes
that were never present in the prerecorded phrase tracks.

`EntertainerAudioTest.exe` exercises the same JUCE/WASAPI backend, procedural
instrument patches, MIDI input callback, and Standard MIDI File parser used by
the x64 gameplay client. It does not load the game, connect to a server, open a
network socket, or require client assets.

The 14 patches use distinct synthesis models for strings, reeds, brass, flute,
horn, plucked strings, percussion, metallic boxes, organ, bells, and flanged
voices. They are not General MIDI samples and are intended as a tunable first
pass at recognizable Star Wars Galaxies instrument families.

Build and launch it from PowerShell:

```powershell
.\scripts\Run-EntertainerAudioTest.ps1
```

Pass a MIDI file to start playback immediately:

```powershell
.\scripts\Run-EntertainerAudioTest.ps1 -MidiFile C:\music\sequence.mid
```

The harness opens the default Windows audio output and the first available MIDI
input. Press `F2` to rescan and cycle through connected MIDI inputs.

## Controls

- Notes: `Z S X D C V G B H N J M , L . ; / Q 2 W 3 E R 5`
- `[` / `]`: shift the octave
- `Page Up` / `Page Down`: select one of the 14 entertainer patches
- `F2`: rescan and select the next MIDI controller
- `F4`: choose and play a `.mid` or `.midi` file
- `F8`: pause or resume MIDI-file playback
- `F9`: stop MIDI-file playback
- `Space`: all notes off
- `Escape`: exit

Keyboard notes respond only while the harness console is focused. MIDI input
continues while the harness is running. MIDI files use the same format, size,
duration, event-count, and event-density validation as the gameplay client.

This harness validates local synthesis and input behavior only. Flourish command
authorization, animation, observer relay, and multiplayer synchronization still
require a server-backed gameplay test.
