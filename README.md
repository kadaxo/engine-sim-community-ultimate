# Engine Simulator: community fork collection

![Engine Simulator](docs/public/screenshots/screenshot_v03.png?raw=true)

A real-time combustion engine simulator that produces engine sound and simulates how an engine responds. It was written by [AngeTheGreat (Ange Yaghi)](https://github.com/ange-yaghi/engine-sim), who no longer maintains the source code. This repository gathers fixes and features from the many community forks of that code, plus new work, into one version that builds and passes its tests.

This is an unofficial collection. It isn't made or endorsed by AngeTheGreat, and it isn't the [Engine Simulator community edition](https://github.com/Engine-Simulator/engine-sim-community-edition), which is a separate project.

It is not a scientific tool: it is designed to sound and respond like an engine, not to give accurate figures for engineering or tuning.

## What's in it

- **Engine builder.** A web page ([tools/engine-builder/index.html](tools/engine-builder/index.html), or `F2` in the simulator) with an ordinary form that writes engine scripts for 30 layouts, from singles, V-twins and crossplane fours to V12s and aircraft radials, and reopens them for editing. It works offline and with screen readers.
- **Game audio export.** `engine-sim-export` renders seamless rpm loops, one-shots and rpm sweeps as Ogg, WAV and MP3 with a JSON manifest, ready for Unity, Unreal, Godot, FMOD or Wwise ([docs/game-audio-export.md](docs/game-audio-export.md)).
- **About 100 engines**, including dozens of community engine scripts, each checked to compile and run, and 7 extra UI themes.
- **Forced induction**: turbochargers (with lag, wastegate or variable geometry vanes, and blow-off valve) and Roots, twin-screw or centrifugal superchargers, with their whistle, whine and blow-off sounds ([docs/forced-induction.md](docs/forced-induction.md)).
- **Diesel engines**: fuel injected into the cylinders with no throttle, a governor, a pilot injection, and the knock and injector ticks a diesel makes, which the engine builder writes for diesel fuel ([docs/diesel.md](docs/diesel.md)).
- **Driving**: smart gear shifting (rev-matched shifts, `K`), brakes (`.`), average or instant fuel consumption and an odometer.
- **More engine types**: camshaft reduction ratio, `sparks_per_cycle` and intake atmosphere for two-stroke engines and altitude setups, and a VTEC switch that changes cams only while each valve is closed.
- **Steady sound**: an optional loudness-based volume leveler (`L`), fixes for audio underruns and a data race in the synthesizer, and a UDP rpm input ([docs/RPM_SYNC.md](docs/RPM_SYNC.md)).
- **Works with screen readers.** Speech and braille through NVDA, JAWS, Narrator and others, keys that read out every gauge, a spoken engine list, and keyboard alternatives for everything the mouse did. See the [accessibility guide](docs/accessibility.html).
- **Builds on Windows, Linux and macOS** with no Boost dependency, plus fixes to the physics, rendering and memory handling. The macOS app has been tried on an Apple Silicon Mac, and the Linux app on Ubuntu 22.04 (under WSL).

[COMMUNITY.md](COMMUNITY.md) lists where every change came from, and [CHANGELOG.md](CHANGELOG.md) what changed in each release.

## Getting it

**Windows 10 or 11 (64-bit):** download `engine-sim-community-<version>-windows-x64.zip` from the [releases](https://github.com/kadaxo/engine-sim-community-ultimate/releases), unzip it into a folder you can write to (such as Documents, not Program Files) and run `engine-sim-app.exe`. Nothing needs installing: SDL2 and the Visual C++ runtime are in the folder, screen reader output is built in, and `engine-sim-export.exe` is next to the app. The app isn't signed, so Windows may say "Windows protected your PC": choose **More info**, then **Run anyway**.

**macOS (Apple Silicon, macOS 11 or newer):** download `engine-sim-community-<version>-macos-arm64.zip`, unzip it and move `Engine Sim.app` to Applications. The app isn't signed by an Apple developer, so the first time macOS refuses to open it: open **System Settings > Privacy & Security** and choose **Open Anyway** next to the message about Engine Sim. `engine-sim-export` is inside the app, in `Engine Sim.app/Contents/MacOS`. Intel Macs need a build from source.

**Linux (x86-64):** the download needs glibc 2.35 or newer (Ubuntu 22.04, Debian 12, Fedora 36 or later) and the system's SDL2, SDL2_image and OpenGL libraries: `sudo apt install libsdl2-2.0-0 libsdl2-image-2.0-0 libopengl0 libgl1`, `sudo dnf install SDL2 SDL2_image libglvnd-opengl` or `sudo pacman -S sdl2 sdl2_image`. Unpack `engine-sim-community-<version>-linux-x64.tar.gz` and run `./engine-sim-app` in its folder. Screen reader output goes through Speech Dispatcher, as Orca's does (tried with eSpeak NG; not yet with Orca itself).

The [original project's releases](https://github.com/ange-yaghi/engine-sim/releases) are older builds without any of the changes above.

## Controls

Press `F1` (or the KEYS button) in the simulator for a list of these keys.

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
| 6 | Where you listen from: beside the car (default), behind it (the exhaust comes forward, the turbo goes back), in front of it, or inside it (through the body, with the cabin's boom) |
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
| F1 | Show or hide the key list |
| F2 | Open the engine builder in your web browser |
| 7, 8 / 9 | Turn the engine view one way or the other / back to straight |
| T, U | Let the clutch slip more or less |
| Y | Keep the clutch where it is (hold) |
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
| KEYS button | Show or hide the key list (same as F1) |
| BUILDER button | Open the engine builder (same as F2) |
| EXIT button | Exit the program (same as Escape) |

"Scroll" means the mouse wheel, or Page Up and Page Down.

An engine or theme picked with the buttons lasts until the app is closed; edit `assets/main.mr` to change the default. If a script has an error, the current engine keeps running and the message panel points to `error_log.log`.

### Holding a steady rpm

The rpm hold keeps the engine at one speed and measures its power and torque there. Press `H` for hold mode, then `D` to turn on the dyno; the hold only works with the dyno on. Hold `G` and scroll to change the hold speed, which is shown on the `DYNO. SPEED` gauge in the lower left.

## Building from source

The project builds on Windows (Direct3D 11), Linux and macOS (SDL2 + OpenGL 4.1). Boost is no longer needed.

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

**macOS** (Homebrew):

```
brew install cmake sdl2 sdl2_image bison flex
```

libvorbis and LAME are optional: they let `engine-sim-export` write Ogg Vorbis and MP3 (WAV always works). CMake looks for their CMake packages (`Vorbis`, `Ogg`, `mp3lame`), which vcpkg provides; on Linux use `vcpkg install libvorbis mp3lame` and pass vcpkg's toolchain file to CMake. The configure output says which formats were enabled.

[Prism](https://github.com/ethindp/prism) is optional too: it gives the simulator speech and braille output through screen readers (`vcpkg install ethindp-prism:x64-windows-static-md` on Windows, `vcpkg install ethindp-prism` on Linux and macOS, then pass vcpkg's toolchain file to CMake). On Linux it builds glib and glibmm and uses Speech Dispatcher, so the first install takes a while and needs autoconf, automake, libtool and pkg-config. On Linux, speech through Speech Dispatcher with eSpeak NG has been tried, Orca hasn't; on macOS speech is untested; the `speech` job in `.github/workflows/cmake.yml` builds it. Without it the screen reader keys stay silent. Its port needs CMake 4 for MSVC; with an older vcpkg (such as the one bundled with Visual Studio) add `--overlay-ports=cmake/vcpkg-overlay-ports`, which builds it as C17. The configure output says whether screen reader output is enabled.

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

macOS. The script compiler needs Bison 3.2 or newer and macOS ships 2.3, so point CMake at Homebrew's Bison and Flex:

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$(brew --prefix) -DBISON_EXECUTABLE=$(brew --prefix bison)/bin/bison -DFLEX_EXECUTABLE=$(brew --prefix flex)/bin/flex
cmake --build build -j
```

Run `build/engine-sim-app` on Linux and macOS or `build\Release\engine-sim-app.exe` on Windows. The app finds `assets` and the script library by looking in its own folder and the folders above it, so it can be started from anywhere. On Windows the vcpkg toolchain copies the SDL2 DLLs next to the executable. On Linux, scripts in `~/.local/share/engine-sim/assets` take priority over the bundled ones, and `ENGINE_SIM_DATA_ROOT` points the app at a different data directory. On macOS, Homebrew's SDL2 loads SDL3 at startup from a folder macOS doesn't search by default, so start the app with `DYLD_LIBRARY_PATH=$(brew --prefix)/lib build/engine-sim-app`.

`pwsh scripts/package-windows.ps1` (in a Developer PowerShell for Visual Studio) makes the Windows release zip from a Release build and checks that it runs on its own; `scripts/package.sh` does the same for macOS and Linux. `.github/workflows/release.yml` builds all three on GitHub, with the libraries the downloads need (see the comments there).

`ctest --test-dir build -C Release -j <cores>` runs the unit tests and an idle check of every engine (`-L slow` for only those, `-LE slow` for the rest; about two minutes on 16 cores), `engine-sim-export --out <dir>` exports a game audio set for the engine in `assets/main.mr` (or `--script <file.mr>`, see [docs/game-audio-export.md](docs/game-audio-export.md)), and `boost-probe <script.mr> [rpm ...]` runs an engine script headlessly on the dyno (handy for checking a new engine). `shift-probe <script.mr>` drives the car through a set of up- and downshifts and prints whether each one completes.

## Credits

Engine Simulator was created by [AngeTheGreat (Ange Yaghi)](https://github.com/ange-yaghi/engine-sim) and first shown in [this YouTube video](https://youtu.be/RKT-sKtR970). The original project was made possible by its Patreon supporters, who are listed in the [original README](https://github.com/ange-yaghi/engine-sim#patreon-supporters).

The changes collected here come from the community forks and contributors credited in [COMMUNITY.md](COMMUNITY.md).

## License

The code is released under the MIT license; see [LICENSE](LICENSE). Optional libraries keep their own licenses: [Prism](https://github.com/ethindp/prism) (screen reader output) is MPL-2.0, SDL2 is zlib, libvorbis is BSD and LAME is LGPL. Each download has their license texts in its `licenses` folder.
