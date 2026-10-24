# Simple Synth (VST3)

Subtractive synth: 2 oscillators (PolyBLEP), unison, SVF filter + envelope, ADSR, LFO, delay, reverb, 5 factory presets, 8 voices.

## Build with GitHub (no local setup)
1. Create a GitHub repo and upload this folder (keep the .github folder).
2. Open the Actions tab, run "Build VST3" (or push).
3. Download the artifacts (Windows and macOS) and copy the .vst3 folder to:
   - Windows: C:\Program Files\Common Files\VST3
   - macOS: /Library/Audio/Plug-Ins/VST3

## Build locally
Needs CMake 3.22+ and a C++ compiler (Visual Studio 2022 or Xcode).
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release --target SimpleSynth_VST3

## Before selling
- Change COMPANY_NAME, PRODUCT_NAME and the 4-letter codes in CMakeLists.txt.
- JUCE is AGPLv3 or commercial: check the license at juce.com before selling a closed-source plugin.
- macOS builds need code signing/notarization to run on other people's machines.

## Not ported yet
Arpeggiator, step sequencer, drums, mod matrix, chorus/phaser/drive, EQ, custom GUI.
