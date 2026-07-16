# Entertainer Reborn

The x64 client provides three performance commands:

- `/startMidiToFlourish [song]` maps configured keys or MIDI notes to the eight
  original flourish phrases. Without a song argument, it opens a learned-song
  picker.
- `/startMidiToMusic` plays the equipped entertainer instrument from a MIDI
  controller or the separate two-octave performance keyboard.
- `/startMusicFromScript [filename]` plays a Standard MIDI File. Without a file
  argument, it opens the MIDI script picker.

Use `/stopPerformanceMode` or Escape to stop any mode. Script playback also has
a Pause/Resume button on its performance HUD.

## MIDI scripts

Place `.mid` or `.midi` files directly in `<game root>\midi`. The build staging
script and client create this directory automatically. Subdirectories, linked
files, and absolute or relative paths outside this directory are not accepted.

Supported input:

- Standard MIDI File format 0 or 1;
- note on/off, velocity, sustain, and all-notes-off events;
- tempo maps and multiple tracks rendered through the equipped instrument's
  procedural patch;
- up to 4 MiB, 64 tracks, 50,000 playable events, 30 minutes, and 160 playable
  events in any rolling second.

Program changes, pitch bend, aftertouch, arbitrary continuous controllers,
format 2 files, and per-track patch assignment are intentionally ignored or
rejected in this first release.

## Performance input

The Performance options page contains the MIDI device, flourish mappings, and
the independent two-octave instrument keyboard. These mappings are active only
inside Entertainer Reborn modes and never overwrite normal gameplay bindings.
`[` and `]` shift the performance octave while Midi to Music is active.
