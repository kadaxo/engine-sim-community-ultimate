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
| `turbine_ar` | 0.64 | Turbine housing A/R: larger flows more and spools later. With variable geometry, the A/R with the vanes open (a petrol engine needs a larger one than a diesel, see below) |
| `turbine_diameter` | 0 | Turbine wheel diameter; its flow goes with the wheel's area (54 mm flows about 0.15 kg/s at 0.64 A/R, the size that goes with the default compressor). 0 sizes it from `compressor_flow` |
| `variable_geometry` | false | Vanes in the turbine housing (VGT, VNT) instead of a wastegate: closed they act as a small housing and spool the turbo at low rpm, and they open to hold the boost |
| `min_turbine_ar` | 0 | Variable geometry: the A/R with the vanes closed; 0 takes a quarter of `turbine_ar` |
| `exhaust_sound` | `"after_turbine"` | Hears the exhaust past the turbine, which smooths its pulses and takes its share of their pressure, as a muffler does. `"before_turbine"` hears it before (the old default) |
| `turbine_efficiency` | 0.7 | |
| `inertia` | 0.00003 | Rotating assembly (kg m²): more inertia, more lag |
| `bearings` | `"journal"` | `"journal"` or `"ball"` (less friction, quicker spool) |
| `boost` | 100 kPa | Wastegate set point (gauge pressure) |
| `boost_control` | `"electronic"` | `"electronic"` holds the set point; `"spring"` opens the wastegate from 75% of it, so boost creeps up with flow |
| `wastegate` | `"internal"` | `"external"` is larger and vents through its own pipe (heard as a rasp) |
| `wastegate_off_boost` | `"closed"` | `"open"`: electronic control holds the wastegate wide open whenever the manifold is below the outside's pressure (idle, cruise, overrun), as engines with an electric actuator do, and shuts it as the throttle opens. Lower pressure ahead of the turbine off boost, at the cost of about a tenth of a second of spool |
| `twin_scroll` | false | Divided turbine housing: better spool at low flow |
| `blowoff_valve` | true | Vents the charge when the throttle closes |
| `blowoff_type` | `"atmospheric"` | `"atmospheric"` (a sharp hiss), `"recirculating"` (a soft whoosh) or `"none"` (the compressor surges and flutters) |
| `intercooler_effectiveness` | 0.7 | Share of the compression heat the intercooler removes |
| `charge_volume` | 6 L | Volume of the pipes and intercooler |
| `whistle_pitch` | 1.0 | Scales the whistle's pitch (sound only) |
| `compressor_blades` | 6 | Full blades of the compressor wheel (sound only) |
| `splitter_blades` | 6 | Splitter blades between them (sound only) |
| `compressor_inlet` | `"silenced"` | What the compressor draws through: `"silenced"` (a factory airbox with a resonator), `"airbox"` or `"open"` (an open filter). Decides how much of the whistle gets out |
| `bearing_wear` | 0 | Worn shaft bearings, from 0 (new) to 1 (worn out): a whine below the whistle with journal bearings, a rough tone above it with ball bearings |
| `wastegate_rattle` | 0 | A worn wastegate linkage, 0 to 1: the open flapper rattles on each exhaust pulse |
| `boost_leak` | 0 | A hole or split coupler in the charge pipe, as its diameter: it hisses and costs boost |
| `anti_lag` | false | A "bang-bang" anti-lag system (see below) |
| `anti_lag_timing` | -30 deg | Its spark timing (negative: after top dead centre) |
| `anti_lag_throttle` | 0.25 | The throttle opening it holds |
| `anti_lag_min_speed` | 3000 rpm | It works above this engine speed |

### supercharger

| Input | Default | Meaning |
| --- | --- | --- |
| `type` | `"roots"` | `"roots"` or `"twin_screw"` (positive displacement: boost from low rpm) or `"centrifugal"` (boost rises with the square of speed) |
| `displacement` | 1.9 L | Air moved per rotor revolution (Roots and twin-screw) |
| `drive_ratio` | 2.0 | Rotor speed / crankshaft speed (for centrifugal ones, including the gearbox step-up) |
| `compressor_diameter`, `compressor_flow`, `compressor_efficiency`, `max_speed` | 90 mm, 0.5 kg/s, 0.75, 0 | Centrifugal superchargers, as for the turbocharger |
| `compressor_blades`, `splitter_blades`, `compressor_inlet` | 6, 6, `"silenced"` | Centrifugal superchargers, as for the turbocharger |
| `boost_leak` | 0 | As for the turbocharger |
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
  housing and a wastegate gets there at 3000. The control aims the exhaust pressure at what
  holds the boost and trims it from the boost error; it allows for the hotter gas of a petrol
  engine, which needs a smaller expansion for the same work. A petrol engine's exhaust carries
  more energy than a diesel's for the same air, so its vanes need a larger open A/R, or the
  boost runs past the set point with the vanes wide open: the B58 holds its boost on vanes with
  an A/R of 1.3 where its wastegated turbine has 0.55.
* **Turbine inlet temperature**: the simulated exhaust of a spark-fired engine leaves the
  cylinders at 2000 to 2900 K at full load, where a real one reaches the turbine at no more
  than about 1300 K (it loses heat to the cylinder walls, ports and manifold, and the engine's
  control adds fuel to hold the turbine there). The turbine works with the gas as if it were
  that cool, softly limited to 1300 K: it takes less energy per mole and passes more moles for
  a given opening. Diesel exhaust (about 850 K) is unaffected. Wastegated petrol engines need
  a little more exhaust pressure to hold their boost, so they spool somewhat later and make
  2 to 15% less torque than before this was added.
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
* **Whoosh and intake roar** (centrifugal compressors): broadband noise the compressor makes as
  it nears surge, at high pressure ratio and low flow, from the flow stalling in the wheel
  (4 to 12 kHz in car turbos; Dehner et al., ASME J. Turbomach. 2021). It climbs with the cube
  of the tip speed, and comes in fully between about half and a quarter of the compressor's
  choke flow coefficient. The air rushing into the inlet adds a roar around 1.5 kHz that rises
  with the cube of its velocity. Both get out through the inlet like the whistle.
* **Blow-off**: the valve's gas jet. Its speed comes from the charge pressure expanding to the
  outside, its diameter from the vented flow at that speed; its level goes with the diameter
  times the fourth power of the speed (Lighthill) and its peak frequency with a Strouhal number
  of 0.2. A lift from full boost starts as a loud, high hiss that falls in pitch and level as
  the charge empties, and a lift from little boost is quiet. A recirculating valve vents into
  the compressor's inlet, so its jet only gets out through the inlet: with a factory intake it
  is barely heard.
* **Surge flutter**: without a valve the flow reverses through the compressor in deep surge
  cycles at about 0.4 times the Helmholtz frequency of the charge volume with the compressor's
  duct (Greitzer), so larger intercooler pipes flutter slower (a 6 L charge on a 60 mm wheel
  about 15 times a second).
* **External wastegate**: what it bypasses leaves through its own short pipe with no muffler
  instead of going on to the muffled exhaust. The open pipe end radiates the rate of change
  of those pulses, a rasp.
* **Worn bearings** (`bearing_wear`): the rotor moves in its bearings' clearance. In journal
  (floating ring) bearings the inner oil film whirls at 0.35 to 0.45 times the shaft's speed
  once the shaft is fast enough, the "constant tone" that new bearings keep faint: worn, it
  sets in at a lower speed and grows into a whine below the whistle, wandering a little in
  pitch because it isn't locked to the shaft. It is the whole rotor moving, so it comes out
  through the housing rather than only the inlet; worn out it is taken as twice the once per
  revolution tone's source. Worn ball bearings ring at the balls passing a race defect (3.6
  times the shaft) instead, rough and swelling once per revolution. Wear also lets the
  imbalance move the rotor further, which raises the once per revolution tone (twice as loud
  worn out).
* **Wastegate rattle** (`wastegate_rattle`): a worn wastegate arm in its bushing lets the
  flapper move in the play once it is off its seat. The steady part of the exhaust pressure
  holds it to one side and each pulse throws it across: it strikes as the pulse rises and,
  more softly, as it falls back, ringing the flapper and housing at about 2.3, 4.1 and 6.9 kHz.
  Shut, the actuator holds it on its seat. A strong steady flow pins it against the linkage, so
  under full load only the ripple moves it; with little flow (idle, cruise) each pulse throws it
  across freely, which is where a worn wastegate is heard most. The wastegate only opens off
  boost with `wastegate_off_boost: "open"`; otherwise this is the rattle under load. A spring
  actuator holds it more loosely than an electronic one, an external wastegate's poppet rattles
  less, and variable geometry has no wastegate.
* **Boost leak** (`boost_leak`): a sharp-edged hole (discharge coefficient 0.65) between the
  charge and the outside. It vents air the compressor has to make up, so the wastegate shuts
  further, the turbo spools later and the engine makes less torque at the same boost (on the
  example Subaru a 20 mm split coupler leaves 0.47 bar at 2500 rpm instead of 0.98 and costs
  14 to 36% of its torque, where a 6 mm hole costs 1 to 3%). Its jet hisses like the blow-off
  valve's, through the hole's own diameter, with its peak at a Strouhal number of 0.2: a 6 mm
  hole at 1 bar about 11 kHz, a split coupler a broad roar from 2 kHz up.
* **Anti-lag** (`anti_lag`): a rally car's "bang-bang" system. With the driver's throttle below
  `anti_lag_throttle` and the engine above `anti_lag_min_speed`, it holds the throttle that far
  open, fires the spark at `anti_lag_timing` (30 degrees after top dead centre by default),
  cuts it altogether on about a third of the cycles at random, and keeps the blow-off valve
  shut. A cut cycle's whole charge is pushed into the exhaust runner unburnt, where the other
  cycles' late-burning gas keeps it hot (above about 900 K): 5 to 10 ms after the cut spark it
  lights and burns in about 0.3 ms. That pressure rise is the bang, heard in the exhaust like
  any other pulse, irregular because the cuts are, and its energy keeps the turbo spinning so the
  boost is there when the throttle opens again (the example Subaru keeps about 12.5 of its 14 psi
  off the throttle at 4500 rpm, where it otherwise drops to nothing). The engine makes little
  torque meanwhile.
  The burning in the runner only happens while anti-lag is on: other engines' unburnt fuel
  stays unburnt, as it always has, so they don't pop on the overrun.
* **The turbine** absorbs part of the exhaust pulses, so a turbocharged engine sounds more
  muffled than the same engine without one. The exhaust is heard past it: the turbine and
  wastegate take their share of the pressure drop to the outside, so only the rest of the
  pulses go on to the muffler, and the exhaust going through the wheel loses the sharp edges of
  its pulses (a low-pass from 4 kHz down to 700 Hz as more of it goes through). A turbo engine
  idling on closed vanes or a shut wastegate then sounds quieter, not brighter. With
  `exhaust_sound: "before_turbine"` the exhaust is heard before the turbine instead, softened
  by a fixed share (the old default).
* **Where you stand**: the exhaust comes out at the back of the car, and the turbo's sounds (and
  a diesel's knock) from the engine bay at the front. Beside the car both are about as far
  away, and the mix is as described above. Behind it (key 6 in the app, `--listener behind` in
  engine-sim-export) or in front (`--listener front`), the far end is taken as 7.5 m away
  against the near end's 3 m (a 4.5 m car), 8 dB down by spreading, and the car's body shields
  it by another 4 dB or so above 1.5 kHz (an estimate). From behind, the blow-off valve on the
  example Subaru sits about 11 dB lower against the engine than from beside; from in front,
  about 8 dB higher. Compressor wheels usually have 6 full and 6 splitter blades; aftermarket
  wheels come in 5+5, 7+7, 9+0 and 11+0, which moves the blade passing tone
  (`compressor_blades`, `splitter_blades`).

## Headless check

`boost-probe <main.mr> [rpm ...]` holds the dyno at each speed at full throttle and prints boost,
manifold pressure, wastegate (or bypass) opening, shaft speed, charge temperature, dyno torque
and supercharger drive load (for variable geometry, the valve column is the vane opening).
`boost-probe <main.mr> --snap <rpm>` checks the response instead:
it cruises at that speed, snaps the throttle open, lifts off, then idles and revs with no load,
printing boost and shaft speed every 0.1 s. See `assets/probe_*.mr`.

## Notes

* `units.psi` in `es/constants/units.mr` used mass-pounds; it now uses pound-force.
* The exhaust sound used to be taken before the turbine by default, so with the wastegate shut
  at idle a turbocharged engine idled somewhat brighter than the same engine without one. It is
  now taken after the turbine; `exhaust_sound: "before_turbine"` keeps the old sound.
* Upstream source ends at v0.1.11a and has no Wankel engine, so boosted rotaries are not possible here.
