# Diesel engines

An engine with `diesel_injection` runs as a diesel: it has no throttle, the fuel is sprayed into
the cylinders, and it lights by itself in the hot compressed air. Without it, an engine is spark
fired, even with diesel fuel (as the older diesel scripts here are).

## Script

```
diesel_injection injection(
    idle_speed: 800 * units.rpm,
    max_speed: 4600 * units.rpm,
    pilot_injection: true
)
engine.add_diesel_injection(injection)
```

Add it after `engine.add_ignition_module(...)`. The ignition module stays: its events start the
injections, its timing curve is the injection timing (about 2 to 8 degrees before top dead
center at idle and 10 to 20 at full load), and its rev limiter still cuts the injections. Give
the engine a straight pedal (`throttle_gamma: 1.0`): the pedal sets the fuel, which goes with the
torque.

Example: `assets/engines/forced-induction/diesel_2_0_common_rail_vgt.mr`, a 2.0 litre common
rail engine with a variable geometry turbo. The engine builder writes `diesel_injection` when
the fuel is diesel.

| Input | Default | Meaning |
| --- | --- | --- |
| `idle_speed` | 750 rpm | The governor holds it with the fuel when the pedal is up |
| `max_speed` | 0 | Governed speed: the fuel tapers to none over `governor_droop` below it. 0: none, only the rev limiter |
| `governor_droop` | 0.06 | Share of `max_speed` over which the fuel tapers. A wider band shapes the top of the torque curve |
| `start_fuel` | 0.6 | Fuel while cranking, as a share of full |
| `full_load_lambda` | 1.3 | Full fuel: the air in the cylinder over this ratio to the fuel it can burn (the smoke limit). The pedal sets a share of it |
| `injection_duration` | 2 ms | Main injection at full fuel, shorter in proportion at part load. It is a time (the rail pressure sets the rate), so at high rpm it spans more crank angle and burns later |
| `pilot_injection` | true | A small injection before the main one, as in common rail engines. False for an old mechanical pump |
| `pilot_fuel` | 0.03 | The pilot's fuel, as a share of full |
| `pilot_separation` | 15 degrees | From the pilot to the main injection |
| `ignition_delay` | 1.0 | Scales the ignition delay |
| `premixed_share` | 0.7 | Share of the fuel sprayed in during the ignition delay that burns at once |
| `combustion_noise` | 1.0 | Loudness of the engine block's knock (0: silent) |
| `injector_noise` | 1.0 | Loudness of the injector ticks (0: silent) |

The fuel's `max_burning_efficiency` scales the torque, as for any engine; the example uses it
to match a real engine's figures.

## How it works

* **No throttle.** The intakes pass air only and stay wide open, so the manifold sits at the
  outside pressure (or the boost) at idle and on a lift, and the engine slows less when the
  pedal comes up than a petrol engine does. The pedal sets the fuel instead.
* **Governor.** It follows the engine's mean speed, as a flyweight governor does (the crank
  slows down at every compression stroke). With the pedal up it adds fuel below `idle_speed`
  and none above it, so a coasting diesel uses no fuel. While cranking it gives `start_fuel`.
* **Injection.** Each injection's fuel is the pedal's (or governor's) share of the full load
  fuel for the air in the cylinder, so boost raises it, as a boost compensator does. With a
  pilot, the events come `pilot_separation` before the timing curve's angle: the pilot is
  sprayed then, and the main injection at the timing curve's angle.
* **Ignition delay.** Diesel fuel lights after a delay that is shorter in hotter, denser air:
  Hardenberg and Hase's correlation for fuel of cetane number 50, integrated over the
  changing pressure and temperature (Livengood and Wu). Cold, barely compressed air hardly
  lights at all.
* **Two burns.** When the charge lights, the fuel sprayed in during the delay
  (`premixed_share` of it) burns within a fraction of a millisecond: the sharp rise in pressure
  that makes a diesel knock. The rest, and all fuel sprayed in after, burns as it mixes with
  the air, over a few crank degrees. A pilot lights the charge first, so the main injection
  burns as it mixes and the knock is softer. Without a pilot, the whole delay's fuel goes at once.
* **Oxygen.** Fuel burns with the oxygen it needs (the fuel's `molecular_afr` per molecule);
  what the oxygen left can't burn leaves as smoke.

## Sound

The exhaust is the same as any engine's. A diesel adds:

* **Knock**: the cylinders' combustion pressure rise rings the engine block (around 1.4 and
  3 kHz). It follows the simulation: harsh without a pilot, softer with one, and strongest
  against the exhaust at idle, where the exhaust is quiet, as a real diesel's clatter is.
* **Injector ticks**: a short click as each injection starts (two per cycle with a pilot).

Both are in the exhaust's units and get each cylinder's exhaust gain and the impulse
responses' gain, so a script's volume settings move them with the exhaust. Engines without
`diesel_injection` don't make them.

## Checking

`boost-probe <main.mr> --combustion <rpm>` prints the first cylinder's peak pressure and its
fastest rise per crank degree (what makes the knock) at idle and on the dyno at a quarter, half
and full pedal. A modern common rail engine idles at about 2 to 4 bar per degree, an old one
without a pilot at 6 to 10. `boost-probe --idle` shows the idle speed and that the manifold
stays at the outside pressure.

## Limits

* One pilot and one main injection, no post injections, EGR or exhaust aftertreatment.
* The ignition delay doesn't know the coolant temperature: a cold start knocks no harder than a
  warm one beyond what the cylinder's own heat loss does.
* As with the simulator's spark-fired engines, the torque comes out high for the air an engine
  takes in, and full load peak pressures run high (the example reaches about 230 bar against a
  real engine's 160 to 180); set the torque with `max_burning_efficiency`.
