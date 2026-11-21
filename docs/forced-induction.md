# Forced induction (turbochargers and superchargers)

An engine can have one turbocharger or supercharger. Its compressor fills a charge-air volume
(the pipes and intercooler) that every intake's throttle draws from instead of the atmosphere,
so the plenum, runners, cylinders, exhaust and sound all respond to boost on their own. The
model comes from [kyan0s-git/engine-sim](https://github.com/kyan0s-git/engine-sim).

## Script

```
turbocharger turbo(
    compressor_diameter: 60 * units.mm,
    compressor_flow: 0.3 * units.kg / units.sec,
    turbine_ar: 0.64,
    twin_scroll: true,
    boost: 14.0 * units.psi,
    intercooler_effectiveness: 0.6
)
engine.add_forced_induction(turbo)

supercharger blower(
    type: "roots",
    displacement: 2.3 * units.L,
    drive_ratio: 2.4,
    intercooler_effectiveness: 0.6
)
engine.add_forced_induction(blower)
```

Examples in `assets/engines/forced-induction/`: a supercharged GM LS and a twin-scroll turbo
Subaru EJ25, from kyan0s-git's engine designer a Toyota 2JZ-GTE (sequential twin turbos),
a BMW B58 (twin-scroll turbo) and a 2026 Formula 1 V6, and a 2.0 common rail diesel with a
variable geometry turbo ([diesel.md](diesel.md)).

### turbocharger

| Input | Default | Meaning |
| --- | --- | --- |
| `arrangement` | `"single"` | `"single"`, `"twin"` (two in parallel) or `"sequential"` (the second joins above `sequential_speed`) |
| `compressor_diameter` | 60 mm | Compressor wheel tip diameter. Sets the boost at a given shaft speed and the speed limit |
| `compressor_flow` | 0.3 kg/s | The compressor's maximum flow. Unless `turbine_diameter` is set, the turbine is sized from it |
| `compressor_efficiency` | 0.75 | |
| `max_speed` | 0 | Shaft speed limit; 0 takes it from the wheel's tip speed limit (520 m/s) |
| `turbine_ar` | 0.64 | Turbine housing A/R: larger flows more and spools later. With variable geometry, the A/R with the vanes open |
| `turbine_diameter` | 0 | Turbine wheel diameter; its flow goes with the wheel's area (54 mm flows about 0.15 kg/s at 0.64 A/R, the size that goes with the default compressor). 0 sizes it from `compressor_flow` |
| `variable_geometry` | false | Vanes in the turbine housing (VGT, VNT) instead of a wastegate: closed they act as a small housing and spool the turbo at low rpm, and they open to hold the boost |
| `min_turbine_ar` | 0 | Variable geometry: the A/R with the vanes closed; 0 takes a quarter of `turbine_ar` |
| `exhaust_sound` | `"before_turbine"` | `"after_turbine"` hears the exhaust past the turbine, which smooths its pulses and takes its share of their pressure, as a muffler does |
| `turbine_efficiency` | 0.7 | |
| `inertia` | 0.00003 | Rotating assembly (kg m²): more inertia, more lag |
| `bearings` | `"journal"` | `"journal"` or `"ball"` (less friction, quicker spool) |
| `boost` | 100 kPa | Wastegate set point (gauge pressure) |
| `boost_control` | `"electronic"` | `"electronic"` holds the set point; `"spring"` opens the wastegate from 75% of it, so boost creeps up with flow |
| `wastegate` | `"internal"` | `"external"` is larger and vents through its own pipe (heard as a rasp) |
| `twin_scroll` | false | Divided turbine housing: better spool at low flow |
| `blowoff_valve` | true | Vents the charge when the throttle closes |
| `blowoff_type` | `"atmospheric"` | `"atmospheric"` (a sharp hiss), `"recirculating"` (a soft whoosh) or `"none"` (the compressor surges and flutters) |
| `intercooler_effectiveness` | 0.7 | Share of the compression heat the intercooler removes |
| `charge_volume` | 6 L | Volume of the pipes and intercooler |
| `whistle_pitch` | 1.0 | Scales the whistle's pitch (sound only) |
| `compressor_blades` | 6 | Full blades of the compressor wheel (sound only) |
| `splitter_blades` | 6 | Splitter blades between them (sound only) |
| `compressor_inlet` | `"silenced"` | What the compressor draws through: `"silenced"` (a factory airbox with a resonator), `"airbox"` or `"open"` (an open filter). Decides how much of the whistle gets out |

### supercharger

| Input | Default | Meaning |
| --- | --- | --- |
| `type` | `"roots"` | `"roots"` or `"twin_screw"` (positive displacement: boost from low rpm) or `"centrifugal"` (boost rises with the square of speed) |
| `displacement` | 1.9 L | Air moved per rotor revolution (Roots and twin-screw) |
| `drive_ratio` | 2.0 | Rotor speed / crankshaft speed (for centrifugal ones, including the gearbox step-up) |
| `compressor_diameter`, `compressor_flow`, `compressor_efficiency`, `max_speed` | 90 mm, 0.5 kg/s, 0.75, 0 | Centrifugal superchargers, as for the turbocharger |
| `compressor_blades`, `splitter_blades`, `compressor_inlet` | 6, 6, `"silenced"` | Centrifugal superchargers, as for the turbocharger |
| `bypass_valve` | true | Recirculates the delivery at part throttle (Roots and twin-screw) |
| `blowoff_valve`, `blowoff_type` | true, `"atmospheric"` | Centrifugal superchargers |
| `intercooler_effectiveness` | 0.0 | |
| `charge_volume` | 4 L | |
| `whistle_pitch` | 1.0 | |

## How it works

* **Centrifugal compressors** (turbochargers and centrifugal superchargers) are a pressure
  source behind a flow restriction. The source pressure follows from the Euler work of the
  impeller, `PR = (1 + eta * sigma * U^2 / (c_p * T1))^(gamma / (gamma - 1))`, so boost depends
  on the wheel's tip speed `U` and drops as the flow nears the compressor's capacity. The
  impeller absorbs `sigma * U^2` of work per kilogram of air it moves.
* **Positive displacement superchargers** (Roots and twin-screw) deliver their displacement
  every revolution; some leaks back through the rotor clearances (more for Roots than
  twin-screw), so boost builds from low rpm.
* **The turbine** is the restriction at each exhaust outlet. It extracts work from the exhaust
  leaving through it; the shaft's speed follows the turbine power minus the compressor and
  bearing power, which gives the lag. The air the compressor moves (including what the
  blow-off valve vents after a lift) slows the shaft, so it winds down within about a second
  and has to spool again next time.
* **The wastegate** bypasses the turbine to hold the boost set point. The electronic control
  is a proportional-integral loop on the share of the exhaust that bypasses the turbine (which
  the boost follows evenly, unlike the valve opening). Its integral only acts near the set
  point, and the valve starts to open as the boost rises quickly towards it, so the boost
  holds steady and overshoots little after a snap of the throttle.
* **Variable geometry** turbines have no wastegate. The same control moves their vanes
  instead: closed below the set point (a quarter of the A/R by default, so the turbo spools
  early) and opening to hold it. With the vanes open, the turbine passes the compressor's whole
  flow (twice a wastegated turbine's), and nearly closed vanes lose some efficiency. In the
  diesel example, the vanes give full boost from 1750 rpm, where the same turbo with a fixed
  housing and a wastegate gets there at 3000.
* **Turbine size**: a turbine's flow scales with its housing's A/R and its wheel's area. A
  smaller turbine raises the exhaust pressure ahead of it, which spools the turbo sooner and
  costs power at the top.
* **Superchargers** load the crankshaft with their drive power (the dyno shows the loss).
* **Air**: the compressor draws from the intakes' atmosphere (`atmosphere_pressure` and
  `atmosphere_temperature` of the intake, standard atmosphere by default), and boost is
  measured against it.
* **Gauges**: the manifold pressure gauge reads boost, and the screen reader's air readout
  (`F8`) includes it.

## Sound

The synthesizer makes the forced induction sounds at the audio rate, following the loudness
of the engine:

* **Turbo whistle**: the compressor's tones, all locked to its shaft (Raitor and Neise, *Sound
  generation in centrifugal compressors*, JSV 2008):
  * once per revolution, from the wheel's residual imbalance: the whistle usually heard from a
    car, at 1.2 to 4.5 kHz. Its pressure goes with the wheel diameter times the tip speed squared;
  * the blades passing: the full blades at the inducer, and with the splitters at the exducer
    (often 10 to 20 kHz on a car's turbo). It rises with the cube of the tip speed and with the
    air flow;
  * buzz-saw: once the flow meets the inducer tips faster than sound, a spread of shaft
    harmonics below the blade passing tone.

  How much gets out depends on the inlet. An airbox takes about 10 dB off at 1 kHz and 15 dB
  from 4 kHz up; a factory intake's resonator takes about another 14 dB from 0.8 to 4 kHz, so a
  stock car keeps the once per revolution whistle in and lets out a faint, high blade passing
  tone, while an open filter lets both out. The levels follow the loudness of the engine
  (the exhaust's level in the synthesizer isn't the same loudness from one engine script to
  another, so it can't carry an absolute level).
* **Supercharger whine**: the rotor lobe passing frequency with its harmonics (Roots and
  twin-screw), or the same compressor tones as a turbo's (centrifugal). It follows rotor speed and boost.
* **Blow-off**: a hiss (or a softer whoosh for a recirculating valve) while the valve vents,
  louder the more boost it releases, so a lift from full boost is heard but cruising with the
  valve open is quiet. Without a valve the compressor surges, heard as a flutter.
* **External wastegate**: a rasp while it vents.
* **The turbine** absorbs part of the exhaust pulses, so a turbocharged engine sounds more
  muffled than the same engine without one. By default the exhaust is heard before the
  turbine, softened by a fixed share. With `exhaust_sound: "after_turbine"` it is heard past
  it: the turbine and wastegate take their share of the pressure drop to the outside, so only
  the rest of the pulses go on to the muffler, and the exhaust going through the wheel loses
  the sharp edges of its pulses (a low-pass from 4 kHz down to 700 Hz as more of it goes
  through). A turbo engine idling on closed vanes or a shut wastegate then sounds quieter, not
  brighter.

## Headless check

`boost-probe <main.mr> [rpm ...]` holds the dyno at each speed at full throttle and prints boost,
manifold pressure, wastegate (or bypass) opening, shaft speed, charge temperature, dyno torque
and supercharger drive load (for variable geometry, the valve column is the vane opening).
`boost-probe <main.mr> --snap <rpm>` checks the response instead:
it cruises at that speed, snaps the throttle open, lifts off, then idles and revs with no load,
printing boost and shaft speed every 0.1 s. See `assets/probe_*.mr`.

## Notes

* `units.psi` in `es/constants/units.mr` used mass-pounds; it now uses pound-force.
* By default the exhaust sound is taken before the turbine, so with the wastegate shut at idle
  a turbocharged engine idles somewhat brighter than the same engine without one;
  `exhaust_sound: "after_turbine"` avoids that. The default stays as it was so the existing
  engines sound the same.
* Upstream source ends at v0.1.11a and has no Wankel engine, so boosted rotaries are not possible here.
