# Engine Simulator: community fork collection

![Engine Simulator](docs/public/screenshots/screenshot_v01.png?raw=true)

A real-time combustion engine simulator that produces engine sound and simulates how an engine responds. It was written by [AngeTheGreat (Ange Yaghi)](https://github.com/ange-yaghi/engine-sim), who no longer maintains the source code. This repository gathers fixes and features from the many community forks of that code, plus new work, into one version that builds and passes its tests.

This is an unofficial collection. It isn't made or endorsed by AngeTheGreat, and it isn't the [Engine Simulator community edition](https://github.com/Engine-Simulator/engine-sim-community-edition), which is a separate project.

It is not a scientific tool: it is designed to sound and respond like an engine, not to give accurate figures for engineering or tuning.

## What's in it

- **Engine builder.** A web page ([tools/engine-builder/index.html](tools/engine-builder/index.html)) with an ordinary form that writes engine scripts for 14 layouts, from singles to V12s, and reopens them for editing. It works offline and with screen readers.
- **Game audio export.** `engine-sim-export` renders seamless rpm loops, one-shots and rpm sweeps as Ogg, WAV and MP3 with a JSON manifest, ready for Unity, Unreal, Godot, FMOD or Wwise ([docs/game-audio-export.md](docs/game-audio-export.md)).
- **About 100 engines**, including dozens of community engine scripts, each checked to compile and run, and 7 extra UI themes.
- **Forced induction**: superchargers and turbochargers ([docs/forced-induction.md](docs/forced-induction.md)).
- **Driving**: smart gear shifting (rev-matched shifts, `K`), brakes (`.`), average or instant fuel consumption and an odometer.
- **More engine types**: camshaft reduction ratio, `sparks_per_cycle` and intake atmosphere for two-stroke and steam engines and altitude setups, and a VTEC switch that changes cams only while each valve is closed.
- **Steady sound**: an optional loudness-based volume leveler (`L`), fixes for audio underruns and a data race in the synthesizer, and a UDP rpm input ([RPM_SYNC.md](RPM_SYNC.md)).
- **Works with screen readers.** Speech and braille through NVDA, JAWS, Narrator and others, keys that read out every gauge, a spoken engine list, and keyboard alternatives for everything the mouse did. See the [accessibility guide](docs/accessibility.html).
- **Builds on Windows and Linux** with no Boost dependency, plus fixes to the physics, rendering and memory handling. macOS builds are set up but untested.

[COMMUNITY.md](COMMUNITY.md) lists where every change came from.

## Getting it

There are no ready-made downloads of this collection yet, so for now it has to be built from source (see [Building from source](#building-from-source)). The [original project's releases](https://github.com/ange-yaghi/engine-sim/releases) are older builds without any of the changes above.

## Controls

| Key/Input | Action |
| :---: | :---: |
| A | Toggle ignition |
| S | Hold for starter |
| D | Enable dyno |
| H | Enable RPM hold (see below for instructions) |
| G + Scroll | Change hold speed |
| F | Enter fullscreen mode |
| I | Display dyno stats in the information panel |
| Shift | Clutch (hold spacebar at the same time to slowly engage/disengage) |
| Up Arrow | Up Gear |
| Down Arrow | Down Gear |
| . | Brake (hold) |
| K | Toggle smart shifting (off by default): the clutch and throttle are worked for you to rev-match each shift, at most half a second |
| L | Cycle the volume leveler: ORIGINAL (default, the original peak leveler), OFF (fixed gain with only a limiter, so revving gets squashed hard) and LOUDNESS (steady loudness from idle to full revs) |
| Z + Scroll | Volume |
| X + Scroll | Convolution Level |
| C + Scroll | High frequency gain |
| V + Scroll | Low frequency noise |
| B + Scroll | High frequency noise |
| N + Scroll | Simulation frequency |
| M | Increase view layer |
| , | Decrease view layer |
| Enter | Reload engine script |
| Escape | Exit the program |
| Q, W, E, R | Change throttle position |
| Space + Scroll | Fine throttle adjustment |
| 1, 2, 3, 4, 5 | Simulation time warp |
| Tab | Change screen |
| P | Toggle Average/Instant fuel consumption |
| Page Up / Page Down | Same as one step of the mouse wheel, with any of the keys above that use Scroll |
| O | Open an engine script (same as the LOAD ENGINE button). With speech on, a spoken list of engines by name instead (see the accessibility guide) |
| [ and ] | Previous or next engine, in the same order as the engine list (the two keys to the right of P) |
| F5, F6, F7, F8, F9, F11, F12 | Screen reader: read out the engine, vehicle, dyno, air, fuel, engine info and sound settings |
| Home | Screen reader: repeat the last announcement |
| J | Screen reader: change how much is announced, or turn speech off (also turns on the Windows voice without a screen reader) |
| F4 | Open the accessibility guide |
| LOAD ENGINE button | Pick an engine script (`.mr`) to run instead of the one in `main.mr` |
| LOAD THEME button | Pick a theme script (`assets/themes`) |
| RELOAD button | Reload the current script (same as Enter) |
| EXIT button | Exit the program (same as Escape) |

"Scroll" means the mouse wheel, or Page Up and Page Down.

An engine or theme picked with the buttons lasts until the app is closed; edit `assets/main.mr` to change the default. If a script has an error, the current engine keeps running and the message panel points to `error_log.log`.

### Holding a steady rpm

The rpm hold keeps the engine at one speed and measures its power and torque there. Press `H` for hold mode, then `D` to turn on the dyno; the hold only works with the dyno on. Hold `G` and scroll to change the hold speed, which is shown on the `DYNO. SPEED` gauge in the lower left.

## Building from source

The project builds on Windows (Direct3D 11) and Linux (SDL2 + OpenGL). The macOS build (SDL2 + OpenGL) is set up in the GitHub workflow but hasn't been tried yet. Boost is no longer needed.

### Step 1 - Clone the repository
```git clone --recurse-submodules <url of this repository>```

If you already cloned it, run `git submodule update --init`. A few fixes for the pinned delta-studio submodule are applied automatically when CMake configures the project, see [patches/README.md](patches/README.md).

### Step 2 - Install the dependencies

You need CMake, a C++17 compiler, SDL2, SDL2_image, Flex and Bison.

**Windows** (Visual Studio 2022): install SDL2 with [vcpkg](https://vcpkg.io) and Flex/Bison with [WinFlexBison](https://github.com/lexxmark/winflexbison) (for example `choco install winflexbison3`):

```
vcpkg install sdl2:x64-windows sdl2-image:x64-windows libvorbis:x64-windows mp3lame:x64-windows
```

**Linux** (Debian/Ubuntu):

```
sudo apt install build-essential cmake libsdl2-dev libsdl2-image-dev flex bison
```

**macOS** (Homebrew, untested):

```
brew install cmake sdl2 sdl2_image bison flex
```

libvorbis and LAME are optional: they let `engine-sim-export` write Ogg Vorbis and MP3 (WAV always works). CMake looks for their CMake packages (`Vorbis`, `Ogg`, `mp3lame`), which vcpkg provides; on Linux use `vcpkg install libvorbis mp3lame` and pass vcpkg's toolchain file to CMake. The configure output says which formats were enabled.

[Prism](https://github.com/ethindp/prism) is optional too: it gives the simulator speech and braille output through screen readers (`vcpkg install ethindp-prism:x64-windows-static-md` on Windows, `vcpkg install ethindp-prism` on Linux and macOS, then pass vcpkg's toolchain file to CMake). On Linux it builds glib and glibmm and uses Speech Dispatcher, so the first install takes a while and needs autoconf, automake, libtool and pkg-config. Speech on Linux and macOS is untested; the `speech` job in `.github/workflows/cmake.yml` builds it. Without it the screen reader keys stay silent. Its port needs CMake 4 for MSVC; with an older vcpkg (such as the one bundled with Visual Studio) add `--overlay-ports=cmake/vcpkg-overlay-ports`, which builds it as C17. The configure output says whether screen reader output is enabled.

### Step 3 - Build and Run

Windows, from the root directory of the project:

```
cmake -S . -B build -A x64 -DCMAKE_TOOLCHAIN_FILE=<vcpkg root>/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

Linux:

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

macOS (untested). The script compiler needs Bison 3.2 or newer and macOS ships 2.3, so point CMake at Homebrew's Bison and Flex:

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$(brew --prefix) -DBISON_EXECUTABLE=$(brew --prefix bison)/bin/bison -DFLEX_EXECUTABLE=$(brew --prefix flex)/bin/flex
cmake --build build -j
```

Run the app with the build directory as the working directory so that it finds `../assets`: `cd build`, then `./engine-sim-app` on Linux and macOS or `Release\engine-sim-app.exe` on Windows (Visual Studio's debugger already uses that directory). On Windows the vcpkg toolchain copies the SDL2 DLLs next to the executable. On Linux, scripts in `~/.local/share/engine-sim/assets` take priority over the bundled ones, and `ENGINE_SIM_DATA_ROOT` points the app at a different data directory.

With CMake 4 add `-DCMAKE_POLICY_VERSION_MINIMUM=3.5`, since some submodules still declare a very old minimum version.

`ctest --test-dir build -C Release` runs the unit tests, `engine-sim-export --out <dir>` exports a game audio set for the engine in `assets/main.mr` (or `--script <file.mr>`, see [docs/game-audio-export.md](docs/game-audio-export.md)), and `boost-probe <script.mr> [rpm ...]` runs an engine script headlessly on the dyno (handy for checking a new engine). `shift-probe <script.mr>` drives the car through a set of up- and downshifts and prints whether each one completes.

On Apple Silicon Macs, closed-throttle (off-load) sound currently renders much louder than on Windows and Linux, while full throttle matches. The cause isn't known yet, so the test that compares the two is skipped there.

## Credits

Engine Simulator was created by [AngeTheGreat (Ange Yaghi)](https://github.com/ange-yaghi/engine-sim) and first shown in [this YouTube video](https://youtu.be/RKT-sKtR970). The original project was made possible by its Patreon supporters, who are listed in the [original README](https://github.com/ange-yaghi/engine-sim#patreon-supporters).

The changes collected here come from the community forks and contributors credited in [COMMUNITY.md](COMMUNITY.md).

## License

The code is released under the MIT license; see [LICENSE](LICENSE). Optional libraries keep their own licenses: [Prism](https://github.com/ethindp/prism) (screen reader output) is MPL-2.0, SDL2 is zlib, libvorbis is BSD and LAME is LGPL.
