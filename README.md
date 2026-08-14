# Ballance Music Player

Basic music player mod for Ballance, implemented using ImGui and Miniaudio.

**BallanceModLoaderPlus (BMLPlus) 0.3.11+** is required. The old/legacy BallanceModLoader is not supported.

For a detailed overview of the mod's config, see comments ingame.

## SlidingRheostat Modes (Dynamic Speed Auto-Adjustment)

- **0 (Disabled)**: Normal playback speed.
- **1 (Linear)**: Playback speed scales linearly with current ball speed:
  $$\text{PlaybackRate} = 1.0 + (\text{CurrentSpeed} - \text{RefSpeed}) \times \text{Slope}$$
- **2 (Sliding Window)**: Playback speed scales with acceleration/deceleration relative to a historical exponential moving average:
  $$\text{PlaybackRate} = 1.0 + (\text{CurrentSpeed} - \text{AvgSpeed}) \times \text{Slope}$$
  - $\text{AvgSpeed}$ updates as: $\text{AvgSpeed} = \text{AvgSpeed} + (\text{CurrentSpeed} - \text{AvgSpeed}) / (\text{Weight} + 1.0)$, where $\text{Weight}$ is configured by `ExtraParameter1` (default `15.0`).
- **3 (Square Root)**: Playback speed scales with the square root of the velocity difference, with the slope automatically scaled by $2\sqrt{\text{RefSpeed}}$ to match linear sensitivity around the reference speed:
  $$\text{PlaybackRate} = 1.0 + (\sqrt{\text{CurrentSpeed}} - \sqrt{\text{RefSpeed}}) \times \text{Slope} \times 2\sqrt{\text{RefSpeed}}$$
- **4 (Logarithmic)**: Playback speed scales logarithmically, with the slope automatically scaled by $(\text{RefSpeed} + 1)$ to match linear sensitivity around the reference speed:
  $$\text{PlaybackRate} = 1.0 + (\ln(\text{CurrentSpeed} + 1) - \ln(\text{RefSpeed} + 1)) \times \text{Slope} \times (\text{RefSpeed} + 1)$$
- **5 (Squared)**: Playback speed scales quadratically to provide maximum speed sensitivity, with the slope automatically scaled by dividing it by $2\text{RefSpeed}$ to keep adjustments reasonable around the baseline reference speed:
  $$\text{PlaybackRate} = 1.0 + (\text{CurrentSpeed}^2 - \text{RefSpeed}^2) \times \frac{\text{Slope}}{2\text{RefSpeed}}$$

## Exposed DLL API Functions

Other mods can control the music player visibility/state via standard C-linkage APIs:

```cpp
extern "C" {
    // Enable or disable the entire music player (true by default)
    MOD_EXPORT void SetMusicPlayerEnabled(bool enabled);
    MOD_EXPORT bool IsMusicPlayerEnabled();

    // Show or hide the ImGui floating dashboard window
    MOD_EXPORT void SetMusicPlayerOpen(bool open);
    MOD_EXPORT bool IsMusicPlayerOpen();
    MOD_EXPORT void ToggleMusicPlayerOpen();
}
```

## Building the Mod

1. Configure using CMake:
   ```bash
   cmake -B build -DBML_DIR=<path-to-bmlplus-files> -DVirtoolsSDK_DIR=<path-to-vtsdk-files> . -G "Visual Studio 17 2022"
   ```
2. Build the project:
   ```bash
   cmake --build . --config Release
   ```
3. Copy `BallanceMusicPlayer.bmodp` to your Ballance `ModLoader/Mods/` directory.
