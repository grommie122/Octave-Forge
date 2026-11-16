# Octave Forge

Octave Forge is a Windows x64 VST3 pitch shifter. Down Tune shifts all three voices from 0 to -12 semitones. The fundamental, octave up, and octave down have independent levels.

## Build

Install CMake and Visual Studio with Desktop development with C++. From
PowerShell in this folder, run:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

CMake fetches JUCE. To build with an existing JUCE checkout, add
`-DJUCE_DIR=C:/path/to/JUCE` to the first command.

## Install in REAPER

Copy the entire `Octave Forge.vst3` folder from
`build/OctaveForge_artefacts/Release/VST3` into a directory in REAPER's VST
search path. On this machine the per-user path is
`%LOCALAPPDATA%\Programs\Common\VST3`.
In REAPER, open Preferences > Plug-ins > VST and click Re-scan. On a track,
open FX and search for Octave Forge.


