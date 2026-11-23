# Audio Underrun Fixes

This branch contains Linux audio underrun diagnostics and fixes for the SDL build.

## Diagnosis

Runtime diagnostics can be enabled with:

```sh
ENGINE_SIM_AUDIO_DIAG=1 ./build/engine-sim-app 2> /tmp/engine-sim-audio.log
```

The diagnostics report:

- synthesizer underrun summaries
- application audio buffer lead drops
- SDL callback delays
- RPM, throttle, frame time, and simulation iteration counts during underrun events

Observed before the fixes: underruns increased at higher RPM, with the app requesting roughly 3000-4300 samples while the synthesizer often had only about 1600-2000 samples available.

## Fix Stack

The fixes were developed as stacked branches:

- `audio-diagnostics-baseline`: add underrun and callback diagnostics.
- `audio-fix-1-sync-synth-buffer`: synchronize writes to the synthesizer audio ring buffer.
- `audio-fix-2-match-synth-lead`: replace the hardcoded 2000-sample synth lead with a 100 ms target lead.
- `audio-fix-3-avoid-step-feedback`: avoid increasing simulation steps when physics processing is already over budget.
- `audio-fix-4-larger-sdl-callback`: increase the SDL callback buffer from 512 to 1024 frames.
- `audio-fix-5-commit-silence-fill`: commit silence-filled samples so the playback write pointer continues advancing after underruns.

## Important Code Changes

### Synchronize Synth Audio Buffer Writes

`Synthesizer::readAudioOutput()` already used `m_lock0`, but `Synthesizer::renderAudio()` wrote to `m_audioBuffer` after releasing that lock. Since `RingBuffer` is not thread-safe, rendered samples are now generated into a local vector and copied to `m_audioBuffer` under `m_lock0`.

### Match Synth Lead To App Lead

The application targets about 100 ms of playback lead. The synthesizer previously generated only up to 2000 samples, which is less than the app can request in one refill. The synth lead is now based on 100 ms of audio at the current audio sample rate.

### Avoid Latency Feedback Under Load

`Simulator::startFrame()` increased simulation steps when synth input latency was below target. Under load this could make frames heavier and worsen underruns. Step increases are now skipped when recent physics processing is already near the frame budget.

### Larger SDL Callback Buffer

The SDL callback buffer was increased from 512 frames to 1024 frames to trade slightly more latency for better underrun tolerance on Linux.

This change is in the `delta-studio` submodule, which is pointed at:

```text
https://github.com/0xsuk/engine-sim-linux-can-delta-studio
```

### Commit Silence Fill

When the synthesizer could not provide all requested samples, it zero-filled the missing part, but the application only committed the non-silent `readSamples` portion. The app now commits the full requested block, including zero-filled silence, so the playback buffer write pointer can recover.

## Result

After applying the full stack, the high-RPM reproduction no longer produced repeated underrun summaries in the diagnostic log. Only the expected startup underrun was observed during the tested run.
