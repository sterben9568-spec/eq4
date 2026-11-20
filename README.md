# RBT EQ — 8-band stereo parametric EQ (prototype 0.3)

An original JUCE-based VST3 project. The workflow takes inspiration from classic mastering EQs: eight dedicated band strips and a compact spectrum/response display on the right. It does **not** copy proprietary bx_digital artwork or assets.

## Implemented

- 8 stereo parametric bands
- Bell, low shelf, high shelf, high-pass and low-pass filter types
- Frequency: 20 Hz–20 kHz; gain: ±18 dB; Q: 0.1–10
- Per-band On and Solo (band-pass audition)
- Real-time FFT spectrum display
- Summed EQ response visualization and draggable frequency/gain nodes
- Host-automatable parameters and saved plugin state (JUCE APVTS)
- VST3 and Standalone build targets

## Build on Windows 10/11 x64

Requirements:
1. Visual Studio 2022 with **Desktop development with C++** installed.
2. CMake 3.22 or newer.
3. Internet access for the first build; CMake downloads JUCE 8.0.6 from its official Git repository.

Steps:
1. Extract this folder somewhere writable.
2. Double-click `build_windows.bat`.
3. If successful, the VST3 bundle is at `build-win64/RBT_EQ_artefacts/Release/VST3/RBT EQ.vst3`.
4. Copy the entire `RBT EQ.vst3` bundle into `C:\Program Files\Common Files\VST3\` (not just a file inside it).
5. Restart Studio One and rescan plug-ins if it does not appear automatically.

## Important status

This is a **source project**, not a compiled Windows VST3 binary. It has not yet been built or validated inside Studio One. The current execution environment cannot reach GitHub to fetch JUCE and does not have the Windows MSVC toolchain, so I cannot honestly label a `.vst3` binary as ready. The included Windows build script configures an explicit x64 Visual Studio generator to avoid accidentally producing a 32-bit build.

## Known prototype limitations

- Solo currently auditions the first enabled Solo band if multiple Solo buttons are active simultaneously.
- The response curve is a visual approximation; audio processing uses the actual biquad filters.
- No oversampling, linear-phase mode, or output gain stage in this prototype.
