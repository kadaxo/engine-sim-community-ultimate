# Community contributions

The original repository stopped receiving updates in 2024. This branch collects the work people published in their forks so it isn't lost.

In October 2026 every repository in the fork network was checked: 951 forks, including forks of forks, with all of their branches, tags and pull request refs, plus the open pull requests on the original repository. 116 of them contain commits that aren't upstream, about 1,700 commits in all. In November 2026 the network was checked again for forks created or pushed to since then (eight, with about 70 new commits). The sections below list what was merged.

Merged work keeps its authors. Fork branches are merged as they are, single commits are cherry-picked with `-x`, and files taken out of larger commits are committed under the original author with the source commit noted in the message. `git log --format='%an %s'` shows who wrote what.

## Merged

### Platform and build

| What | From |
| --- | --- |
| Cross-platform port: SDL/OpenGL on Linux and macOS, GCC/Clang fixes, runtime data paths, discord-rpc as a submodule, plus fixes for divide-by-zero, out-of-bounds and leaks on engine reload | [bobsayshilol/engine-sim](https://github.com/bobsayshilol/engine-sim) `sdl-build` |
| Linux audio underrun fixes, a data race in the synthesizer's audio buffer, opt-in audio diagnostics, UDP RPM input (`RPM_SYNC.md`, `AUDIO_FIXES.md`) | [0xsuk/engine-sim-linux-can](https://github.com/0xsuk/engine-sim-linux-can) |
| MSVC fix for `collision_detector.cpp` (delta-studio patch 0001) | David ([Creesic/delta-studio](https://github.com/Creesic/delta-studio) `macos-port`) |
| Release builds no longer need the debug D3DX DLL (delta-studio patch 0004) | [Danniel-Porto/delta-studio](https://github.com/Danniel-Porto/delta-studio) `release-directx-linking` |
| Windows CI provisioning (Chocolatey WinFlexBison, vcpkg SDL2) | [jacksonelectrical/engine-sim](https://github.com/jacksonelectrical/engine-sim) |
| `.gitignore` for CLion and Visual Studio | [TheBengineer/engine-sim](https://github.com/TheBengineer/engine-sim) |

Added on top: LOAD ENGINE / RELOAD / LOAD THEME / EXIT buttons in the header, modelled on the closed-source v0.1.14a release. Loading engines uses a native file picker (zenity on Linux, as in [Fraxoor/engine-sim-linux](https://github.com/Fraxoor/engine-sim-linux)) and includes Fraxoor's finding that the script compiler's static output must be cleared between loads. An optional volume leveler levels by loudness (RMS over a sliding 0.3 s window) with a separate peak limiter, instead of by peak level: idle, part throttle and full revs come out at about the same loudness (within ~1.5 dB on the engines tested, where the old leveler differed by up to 14 dB) and the output no longer clips. The original peak leveler stays the default; `L` cycles between it, no leveling and the loudness leveler. Screen reader support: announcements and gauge readouts through NVDA, JAWS, Narrator and others via [Prism](https://github.com/ethindp/prism), Page Up/Page Down in place of the mouse wheel, `O` to open an engine, spoken script errors and dyno summaries with a CSV report, and delta-studio patch 0009 so input injected by assistive software isn't dropped ([docs/accessibility.html](docs/accessibility.html)). An engine builder ([tools/engine-builder](tools/engine-builder/index.html)): an accessible web form that writes engine scripts based on Ange's stock engines, with crank pins computed from the firing order; every preset is checked on the dyno by `ctest` (label `slow`).

Fixes made while merging: Direct3D and Win32 build errors in the SDL port of delta-studio, a global buffer overflow in its device registry (found with AddressSanitizer), SDL2 header discovery with vcpkg, a Winsock version of the RPM sync receiver, the boost-probe tool on the port's compiler API, smart shifting after the simulator refactor (and shifts that never finished when the throttle was held, which left the clutch open and the throttle latched), and a test suite that passes. See `patches/README.md` for the delta-studio patches.

### Simulation

| What | From |
| --- | --- |
| Superchargers and turbochargers (`docs/forced-induction.md`), `units.psi` fix, `boost-probe` headless tool | [Oblongy/engine-sim](https://github.com/Oblongy/engine-sim) |
| Camshaft `reduction_ratio` (2-stroke and steam engines) and intake `atmosphere_pressure`/`atmosphere_temperature` | Alexey Egorov ([Evil-Spirit/engine-sim](https://github.com/Evil-Spirit/engine-sim), upstream PR #309) |
| Smart (rev-matched) shifting (behind the `K` toggle, off by default), instant/average fuel consumption (`P`), odometer, clutch and throttle logic moved out of the UI | Oleg Solovyov ([Mc-Pain/engine-sim](https://github.com/Mc-Pain/engine-sim)) |
| Articulated-rod geometry, road drag when reversing, ignition events at the crank-angle wrap, crank-side rod damping, frame-rate independent UI smoothing and controls, rendering guards | Samuel ([Samzax/engine-sim](https://github.com/Samzax/engine-sim), 12 commits) |
| GaussianFilter out-of-bounds read, ConvolutionFilter null dereference, Gaussian test | [danieljsinclair/engine-sim](https://github.com/danieljsinclair/engine-sim) |
| Simulator teardown leak, uninitialized synthesizer latency, and `engine-sim-export`, a headless exporter that renders seamless RPM loops, one-shots and RPM sweeps as Ogg Vorbis, WAV and MP3 with a JSON manifest for games ([docs/game-audio-export.md](docs/game-audio-export.md)). Ported with optional encoders (WAV-only without libvorbis/LAME) and support for engine files from `assets/engines` | 4FX ([forfex/engine-sim](https://github.com/forfex/engine-sim)) |
| `checkForDiscontinuity` typo, simpler `modularDistance` | wardio (upstream PR #216) |
| Brakes (hold `.`, `max_brake_force` on vehicles), and the full `smooth_01`–`smooth_49` series in the impulse response library | barolig ([Sness00/engine-sim](https://github.com/Sness00/engine-sim)) |
| `sparks_per_cycle` for two-stroke ignition, and leaks of the displacement scratch arrays, jitter history and crankshaft links on every engine load | [karpuzikov](https://github.com/karpuzikov/engine-sim) |
| VTEC switches each valve between cams only while it is closed, so the lift no longer jumps mid-opening | [sillysigmabollocks](https://github.com/sillysigmabollocks/engine-sim-test) |

Already covered by the port, so not merged separately: Ange's own dyno torque index fix (still only in a PR ref), KeesTucker's stale-assert removal, the earlier Linux/compiler fixes by xorgy (PR #220), yangosoft, Pinim04, Fraxoor, LukasACH, Beacroxx and arf20, and Hochmah's teardown, RingBuffer and clang fixes.

### Engines and themes

Each script was compiled and run headlessly (cranks, idles and holds 1500 rpm on the dyno) before being added.

| Engines | From |
| --- | --- |
| Fiat 1.8 16V HGT, BMW S54, Subaru EJ22 | Ilija (upstream PRs #146, #147, #165) |
| BMW M3 E46 | eYuM (upstream PR #412) |
| Volvo 2.5 I5 | Kestin Goforth ([kForth](https://github.com/kForth/engine-sim)) |
| McLaren-Honda MP4/4 (RA168E) | Finn Ericson ([ioramastudio](https://github.com/ioramastudio/engine-sim-APPLE_ARM)) |
| LADA Niva VAZ-21214, UAZ-469, Ural IMZ-8.103, ZIL-130 | [tjvjk](https://github.com/tjvjk/engine-sim-macos) |
| Steam and small 2-stroke samples (`engines/steam`) | Alexey Egorov ([Evil-Spirit](https://github.com/Evil-Spirit/engine-sim)) |
| Husqvarna 701 | Kees ([KeesTucker](https://github.com/KeesTucker/engine-sim)) |
| Ferrari 499P, Porsche 992 GT3 4.0 flat-6 | Thomas Vaté ([Durb0](https://github.com/Durb0/engine-sim)) |
| Chevy 427 L88, Hemi 392, Viper V10, Lamborghini V10, AMG M177, 540 blown monster truck V8, RB26DETT, Porsche 3.8 flat-6 | Andrew McOlash ([amcolash](https://github.com/amcolash/engine-sim)) |
| Subaru EL15/EL22/EL30/EL37/EL45, I8, big-bang V6, 25 cc/50 cc singles, Ferrari 312T flat-12, Honda GX25, I2, Porsche 993, I3, VW W16, crossplane Yamaha R1, YZ450F | Evert Jans ([ejans](https://github.com/ejans/engine-sim)) |
| BMW S58, Lamborghini V12, VW 1.0 TSI, Mitsubishi 4G63, Audi S1 quattro, Cosworth TJ V10, Toyota 1KR-FE, 4A-GE ITB (folders are named after the script authors) | jotalamp / Joonas Lampinen ([jotalamp](https://github.com/jotalamp/engine-sim)) |
| VQ35DE, 2JZ with gearbox (and VVT), Toyota 2RZ-EF, Jeep 4.0 I6, SR20DE, W3; RB25DET, Kohler V8/V12, Kubota D722, 4A-GE, Mini R53, MR20DE | Vrobocio ([DDev247](https://github.com/DDev247/engine-sim), [mod-loader](https://github.com/mod-loader/engine-sim-modloader)) |
| I3/I5/I7 star, Audi I5 | StanTheAwesomeMan ([Beacroxx](https://github.com/Beacroxx/engine-sim)) |
| Volvo D13 diesel | [FilipeSchultz](https://github.com/FilipeSchultz/engine-sim) |
| BMW M62B44-like V8 | [Talhasarac](https://github.com/Talhasarac/engine-sim-sound-exporter) |
| Toyota 4A-GE (AE86) | 4FX ([forfex](https://github.com/forfex/engine-sim)) |
| Porsche 992 GT3 | csaba.hegedus ([csabigee](https://github.com/csabigee/engine-sim)) |
| Subaru EJ25 street exhaust | [Samzax](https://github.com/Samzax/engine-sim) |
| GM LS supercharged, Subaru EJ25 twin-scroll turbo | [Oblongy](https://github.com/Oblongy/engine-sim) |
| MTU MB 871 Ka-501 tank diesel (by pilot01 and LTSP6.1.1 on Archangel Motors' template) | barolig ([Sness00/engine-sim](https://github.com/Sness00/engine-sim)) |
| 2005 Toyota Corolla XRS 2ZZ-GE (lift switch at 6000 rpm) | [sillysigmabollocks](https://github.com/sillysigmabollocks/engine-sim-test) |

Themes: metric versions of the stock themes (`themes/*_metric.mr`) by Abdullah-Ajeebi (upstream PR #376), and Catppuccin by StanTheAwesomeMan (Beacroxx).

