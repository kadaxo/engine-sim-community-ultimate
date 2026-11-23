# Changelog

What changed in each release of the community fork collection. [COMMUNITY.md](COMMUNITY.md) lists which fork each change came from.

A release's notes on GitHub are its section below: the release workflow copies the section whose heading is the tag's version.

## 0.3.0

### New

- **Diesel engines.** An engine with `diesel_injection` runs as a diesel: no throttle, a governor that holds the idle and cuts the fuel near the top speed, a pilot and a main injection into the cylinders, an ignition delay, and the knock and injector ticks a diesel makes. The engine builder writes diesel engines, and `forced-induction/diesel_2_0_common_rail_vgt.mr` is a 2.0 common rail turbodiesel ([docs/diesel.md](docs/diesel.md)).
- **Turbo sounds from the turbo.** The whistle is the compressor's own tones (once per revolution, the blades passing and, at high tip speeds, buzz-saw), and the whoosh near surge, the blow-off valve's hiss, surge flutter and an external wastegate's rasp come from the airflow. New inputs: `compressor_blades`, `splitter_blades` and `compressor_inlet`. The exhaust is now heard after the turbine (`exhaust_sound: "before_turbine"` keeps the old sound).
- **Variable geometry turbos** (`variable_geometry`, `min_turbine_ar`), which hold their boost on petrol engines too.
- **Turbo faults and anti-lag**, all off by default: worn bearings (`bearing_wear`), a rattling wastegate (`wastegate_rattle`), a boost leak (`boost_leak`) and an anti-lag system (`anti_lag`) with its bangs. `wastegate_off_boost: "open"` holds the wastegate open off boost, as electric actuators do.
- **A throttle before the blower** (`throttle_position: "before"`) for Roots and twin-screw superchargers, as the injector hat on a drag or monster truck engine: the blower draws through the throttle, so it makes little or no boost at light throttle. The monster truck uses it; it made 34 psi at a fifth throttle and now runs in vacuum there, with the same power at full throttle to within about 4%.
- **Supercharger whine** follows the rotors (`rotor_lobes`, `rotor_twist`), the drive gears (`gear_teeth`, `gear_whine`) and `whine_volume`. `rotor_efficiency` sets the rotors' efficiency, and a centrifugal supercharger's drive ratio and impeller now default to a supercharger's, not a turbo's.
- **Where you listen from** (`6` in the app, `--listener` in engine-sim-export): beside the car, behind it, in front of it or inside it. `engine_location: "front" | "mid" | "rear"` places the engine; the bundled 911, Ferrari, Lamborghini, McLaren, Cosworth and Veyron engines are marked.
- **A key list** on `F1` or the KEYS button, and the **engine builder** on `F2` or the BUILDER button.
- engine-sim-export: a full-throttle pull through the gears (`--clips pulls`).
- Engine builder: 30 layouts instead of 14. Engines that fire unevenly: 45 and 90 degree V-twins (Harley-Davidson, Ducati), 180 and 270 degree parallel twins, the crossplane inline 4 (Yamaha R1), V4s with a 180 degree crank (Honda VFR) or Twin Pulse (Ducati), and the odd-firing 90 degree V6 (Buick) and V10 (Dodge Viper). Also a boxer twin, the small-block Chevrolet, Ford and Mercedes V8 firing orders, three-valve heads, and 7, 9 and 18 cylinder aircraft radials, with presets styled on these engines.
- boost-probe: `--idle`, `--crank`, `--starter-speed`, `--combustion` and `--cr` (the compression ratio as simulated). boost-probe and shift-probe now take engine files from `assets/engines` and run from any folder.

### Changed

- Turning the engine view moved from `F1`, `F2` and `F3` to `7`, `8` and `9`.
- Many community engines now have their real compression ratios (they ranged from 5.4:1 to 53:1), and eleven have ordinary cranking speeds instead of starters that ran the engine at idle speed.
- Combustion uses the fuel's own oxygen ratio, so alcohol fuels burn. Petrol engines are unchanged; the monster truck now runs on methanol.
- The exhaust's pipe delays are right at every simulation frequency (they were sized for 10 kHz), which changes engines with uneven pipe lengths a little.
- The B58, 2JZ-GTE and F1 V6 have a gentler pedal, as most engines here do.
- Engines copied from another file have their own names (the EL45, the I2, the 25 cc single, the triple-crank I5/I4/I3, the 2JZ and EJ25 variants).
- The McLaren-Honda runs at 15 kHz; at 4.2 kHz it blew up on the dyno.
- The steam engines are gone: they only ran because of a starter bug.
- `AUDIO_FIXES.md` and `RPM_SYNC.md` moved into `docs`.
- The Linux download is built on Ubuntu 24.04 and needs glibc 2.39 or newer (Ubuntu 24.04, Debian 13, Fedora 40 or later); 0.2.1 ran from Ubuntu 22.04 and Debian 12 up.

### Fixed

- A Roots or twin-screw supercharger made more boost at part throttle than at full (its bypass opened only on vacuum).
- The app and engine-sim-export closed at startup, with no message, from a folder whose name has letters outside Windows' code page (such as Işık on a Western Windows).
- A simulation that blows up (a step too coarse for the engine) is stopped: the app loads the engine again and says why, and engine-sim-export fails the clip.
- A crash at random in long sessions, from the error call stack shared between threads (delta-studio).
- Audio: a rare hang between the audio and simulation threads, refills that waited too long, and dropouts that replayed old sound instead of fading out.
- The dyno hold drove engines that turn counter-clockwise (such as the Honda B18C) backwards.
- Engines with two exhausts or two intakes sounded one of two ways from run to run.
- Four engines idled at 4000 to 8000 rpm with the throttle closed, the UAZ-469 and ZIL-130 didn't load, the Ferrari 499P's starter ran it to redline, and a negative `starter_torque` gave a starter with no speed limit.
- The gas system tests wrote their CSV files into the folder they were run from.
- The engine builder's test checked each preset's charge-air temperature instead of its torque, so it passed any engine that started; and its 5.7 V8 preset stalled when the throttle was lifted at speed.

### Faster

- Checking an engine with boost-probe takes about 8 s instead of 40, and the exhaust convolution costs a twenty-sixth as much for a long impulse response.

## 0.2.1 (2026-11-16)

- Downloads for macOS (Apple Silicon) and Linux (x86-64), built by the release workflow.
- The downloads find their shaders and fonts without a `delta.conf` beside the program.
- The app no longer writes its scene file into its data folder at every start, which failed in read-only installs.

## 0.2.0 (2026-11-16)

The first release of the collection: Engine Simulator with the fixes and features of the community forks, plus new work. See [README.md](README.md) for what's in it and [COMMUNITY.md](COMMUNITY.md) for where each change came from.

## Earlier

Versions up to 0.1.11a are AngeTheGreat's, from the [original project](https://github.com/ange-yaghi/engine-sim/releases).
