# Graphics Reborn: moving work from the CPU to the GPU

Branch `x64-dx11-graphics-reborn`, cut from `x64-dx11-vanilla` after the DX11 port was verified
working in game.

This branch deliberately breaks parity with DX9. `x64-dx11-vanilla` stays the parity branch and is
where fixes to shipped behaviour belong; anything here changes how the client renders.

## Why, with numbers

Measured on the running client, not assumed. The counters are the always-on ones in
`Direct3d11_Metrics`, with per-call timing enabled via `DX11_TIMING_ENABLED` and reporting via the
`reportFrameTiming` config key.

| quantity | measured |
| --- | --- |
| GPU frame time (query ring) | 0.1 – 5 ms |
| DX11 backend CPU, excluding streaming | under 1 ms per frame |
| engine render phase (`beginScene`..`endScene`, minus the backend) | 1 – 8 ms typical |
| engine update phase (`Present` .. next `beginScene`) | 2 – 5 ms typical, 40 – 90 ms on hitches |
| dynamic vertex ring traffic | up to **6.4 MB per frame**, ~1950 appends |
| frame rate, uncapped, outdoors | 180 – 240 fps |

The shape of it: **the GPU is nearly idle and the CPU does the work.** The backend itself is not the
cost -- it is under a millisecond -- so there is nothing to win by making it faster. The win is in
what the engine hands it.

The ring traffic is the clearest signal. 6.4 MB of vertex data per frame is geometry the CPU built
and re-uploaded, and the class responsible names itself: `SoftwareBlendSkeletalShaderPrimitive`.
Every skinned character is transformed on the CPU, every frame. Shadow volumes are the same
pattern -- `ShadowVolume.cpp` computes silhouettes and extrusion into system vertex and index
buffers on the CPU.

## What this buys, honestly

The client already runs at 180 – 240 fps in ordinary scenes. Offloading will not make a quiet street
faster; the frame is already short and the GPU already idle. It pays in scenes with many skinned
characters, which is exactly where the engine's per-character CPU cost multiplies -- crowded
cantinas, combat, cities at peak.

So the target is the worst case, not the average, and the measurement that matters is frame time in
a crowded scene rather than standing in the desert.

## Order of work

Each step ends in a measurement against the same scene, and none of them start before the previous
one is verified in game.

1. **Measure first.** Time `SoftwareBlendSkeletalShaderPrimitive` and `ShadowVolume` separately, per
   frame, in a crowded scene. Everything below is a guess about where the time is until this exists.
   This is the same discipline that found the fog fault: three plausible theories died to
   measurements before the real one showed up.

2. **GPU skinning.** Bone matrices as vertex shader constants, vertex formats carrying indices and
   weights, and a vertex shader per skinned material. This removes both the CPU transform and the
   per-frame upload. It is the largest item and the largest payoff.

3. **GPU shadow volumes, or shadow maps.** Silhouette extrusion in a geometry shader, or replacing
   stencil volumes with shadow maps. Shadow maps are the bigger change and the better end state, and
   they would also fix shadows vanishing at range -- currently a consequence of shadow geometry
   living on the mesh's detail level.

4. **Particles and effects.** Whatever else is still coming through the dynamic ring once skinning
   is gone.

## Constraints

- New shaders are now in scope. They were not on the parity branch: that branch is stock DX9 assets
  translated, and the Graphics Reborn options panel was removed from the client because nothing
  backed it.
- Every step keeps a switch to fall back to the CPU path, so a regression can be bisected in a
  running client rather than by rebuilding.
- `_client` remains the untouched DX9 baseline. It is the only reference for "did this look right
  before", and it has already settled several questions this session.

## Verification

- The zero-invariant counters must stay at zero: dropped draws, blocking staging maps, bake
  readbacks, in-frame creations after warm-up.
- Frame time compared in the same crowded scene, before and after, rather than in the scene that
  happens to be loaded.
- GPU frame time from the query ring should rise as work moves across. If it does not, the work did
  not move.
