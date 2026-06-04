# XNULLX Plugins

Source + CMake build for three XNULLX audio plugins (JUCE):

| Plugin | Type | Formats |
| --- | --- | --- |
| **GrainBrain** | Multiband granular processor (FX) | VST3, AU |
| **O2** | 3-band saturation & width (FX) | VST3, AU |
| **Bunka** | Beat-slicer sampler (Instrument) | VST3, AU |

Built and shipped on Windows (VST3). This repo exists to build the macOS
versions (VST3 + AU) from the same source.

## Requirements

- **CMake** 3.22+
- **macOS:** Xcode + command-line tools (`xcode-select --install`)
- **Windows:** Visual Studio 2022/2026 with the C++ workload
- JUCE is downloaded automatically by CMake (pinned to 8.0.13) — nothing to install.

## Build (all three plugins)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The first configure clones JUCE (one-time, a few minutes). Build one plugin
only with e.g. `cmake --build build --target Bunka_VST3`.

## Where the built plugins land

After a successful build, the bundles are under `build/<Plugin>/<Plugin>_artefacts/Release/`:

- macOS VST3 → `.../VST3/<Plugin>.vst3`  → copy to `~/Library/Audio/Plug-Ins/VST3/`
- macOS AU   → `.../AU/<Plugin>.component` → copy to `~/Library/Audio/Plug-Ins/Components/`

Validate the AU with: `auval -v aufx <PluginCode> XNLX` (use `aumu` for Bunka, the instrument).

## Notes

- Plugin/manufacturer codes are fixed (`XNLX`; GrainBrain `Cl7u`, O2 `O2XN`, Bunka `Bnka`)
  so the macOS builds share identity with the Windows builds.
- For distribution, macOS builds should be code-signed and notarized
  (an Apple Developer account is required for that).
